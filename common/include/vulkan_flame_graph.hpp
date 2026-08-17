// ============================================================================
// Vulkan 1.4 Flame Graph Profiling Engine
// Standardized for Clang 17+ Compiler and Vulkan 1.4 API Specification
// Supports:
//   - RAII-based Scoped CPU Profiling
//   - Vulkan 1.4 Core GPU Timestamp Query Pools (vkCmdWriteTimestamp2 / vkCmdWriteTimestamp)
//   - Hierarchical Call Tree Accumulation & Multi-Frame Aggregation
//   - Standard Folded Stack Profile Export (.folded format for flamegraph.pl / Speedscope)
//   - Chrome Trace / Perfetto Event Export (.json for chrome://tracing)
//   - Self-Contained Interactive HTML / SVG Flame Graph Visualizer
// ============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stack>
#include <mutex>
#include <algorithm>

namespace vk_profiler {

// ----------------------------------------------------------------------------
// Hierarchical Profile Node for Flame Graph generation
// ----------------------------------------------------------------------------
struct ProfileNode : public std::enable_shared_from_this<ProfileNode> {
    std::string name;
    uint64_t totalDurationNs = 0; // Accumulated time in nanoseconds
    uint64_t selfDurationNs = 0;
    uint32_t callCount = 0;
    std::vector<std::shared_ptr<ProfileNode>> children;
    std::weak_ptr<ProfileNode> parent;

    ProfileNode(const std::string& n, std::shared_ptr<ProfileNode> p = nullptr)
        : name(n), parent(p) {}

    std::shared_ptr<ProfileNode> getOrCreateChild(const std::string& childName) {
        for (auto& child : children) {
            if (child->name == childName) {
                return child;
            }
        }
        auto newChild = std::make_shared<ProfileNode>(childName, shared_from_this());
        children.push_back(newChild);
        return newChild;
    }
};

// ----------------------------------------------------------------------------
// Chrome Trace Event Descriptor
// ----------------------------------------------------------------------------
struct TraceEvent {
    std::string name;
    std::string cat;
    char ph; // 'B' (Begin), 'E' (End), 'X' (Complete), 'i' (Instant)
    uint64_t tsMicroseconds;
    uint64_t durMicroseconds;
    uint32_t pid;
    uint32_t tid;
};

// ----------------------------------------------------------------------------
// GPU Timestamp Span
// ----------------------------------------------------------------------------
struct GpuTimestampSpan {
    std::string name;
    uint32_t startQueryIndex;
    uint32_t endQueryIndex;
};

// ----------------------------------------------------------------------------
// Flame Graph Profiler Engine Class
// ----------------------------------------------------------------------------
class FlameGraphProfiler {
private:
    std::string m_sessionName;
    std::shared_ptr<ProfileNode> m_rootNode;
    std::shared_ptr<ProfileNode> m_currentNode;
    std::stack<std::chrono::high_resolution_clock::time_point> m_cpuTimeStack;
    std::vector<TraceEvent> m_traceEvents;
    std::chrono::high_resolution_clock::time_point m_sessionStartTime;
    std::mutex m_mutex;

    // Vulkan GPU Timing
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueryPool m_queryPool = VK_NULL_HANDLE;
    float m_timestampPeriod = 1.0f; // Nanoseconds per GPU clock tick
    uint32_t m_maxGpuQueries = 256;
    uint32_t m_currentGpuQueryIndex = 0;
    std::vector<GpuTimestampSpan> m_gpuSpans;
    bool m_gpuProfilingEnabled = false;

public:
    FlameGraphProfiler(const std::string& sessionName = "Vulkan14_Session")
        : m_sessionName(sessionName) {
        m_rootNode = std::make_shared<ProfileNode>("root");
        m_currentNode = m_rootNode;
        m_sessionStartTime = std::chrono::high_resolution_clock::now();
    }

    ~FlameGraphProfiler() {
        cleanupGpu();
    }

    static FlameGraphProfiler& get() {
        static FlameGraphProfiler instance("Global_Vulkan14_Profiler");
        return instance;
    }

    void setSessionName(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessionName = name;
    }

    // ------------------------------------------------------------------------
    // Vulkan GPU Query Pool Initialization
    // ------------------------------------------------------------------------
    void initGpu(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t maxQueries = 256) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_device = device;
        m_maxGpuQueries = maxQueries;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        m_timestampPeriod = properties.limits.timestampPeriod;

        if (m_timestampPeriod <= 0.0f) {
            m_timestampPeriod = 1.0f;
        }

        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = maxQueries;

        if (vkCreateQueryPool(device, &poolInfo, nullptr, &m_queryPool) == VK_SUCCESS) {
            m_gpuProfilingEnabled = true;
        } else {
            m_gpuProfilingEnabled = false;
        }
    }

    void cleanupGpu() {
        if (m_device != VK_NULL_HANDLE && m_queryPool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_device, m_queryPool, nullptr);
            m_queryPool = VK_NULL_HANDLE;
            m_gpuProfilingEnabled = false;
        }
    }

    void resetGpuFrame(VkCommandBuffer cmd) {
        if (!m_gpuProfilingEnabled) return;
        m_currentGpuQueryIndex = 0;
        m_gpuSpans.clear();
        vkCmdResetQueryPool(cmd, m_queryPool, 0, m_maxGpuQueries);
    }

    // ------------------------------------------------------------------------
    // GPU Scoped Timestamp Recording
    // ------------------------------------------------------------------------
    void beginGpuScope(VkCommandBuffer cmd, const std::string& scopeName) {
        if (!m_gpuProfilingEnabled || m_currentGpuQueryIndex + 2 > m_maxGpuQueries) return;

        uint32_t startIdx = m_currentGpuQueryIndex++;
        uint32_t endIdx = m_currentGpuQueryIndex++;

        m_gpuSpans.push_back({ scopeName, startIdx, endIdx });

        // Vulkan 1.4 Core: TOP_OF_PIPE timestamp
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, startIdx);
    }

    void endGpuScope(VkCommandBuffer cmd) {
        if (!m_gpuProfilingEnabled || m_gpuSpans.empty()) return;
        uint32_t endIdx = m_gpuSpans.back().endQueryIndex;
        // Vulkan 1.4 Core: BOTTOM_OF_PIPE timestamp
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, endIdx);
    }

    // Collect GPU Results after device execution / queue wait
    void resolveGpuResults() {
        if (!m_gpuProfilingEnabled || m_gpuSpans.empty()) return;

        uint32_t totalQueries = m_currentGpuQueryIndex;
        if (totalQueries == 0) return;

        std::vector<uint64_t> results(totalQueries, 0);
        VkResult res = vkGetQueryPoolResults(
            m_device, m_queryPool, 0, totalQueries,
            results.size() * sizeof(uint64_t), results.data(),
            sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        if (res == VK_SUCCESS) {
            beginCpuScope("GPU_Timeline");
            for (const auto& span : m_gpuSpans) {
                if (span.endQueryIndex < results.size() && span.startQueryIndex < results.size()) {
                    uint64_t rawDelta = (results[span.endQueryIndex] >= results[span.startQueryIndex])
                        ? (results[span.endQueryIndex] - results[span.startQueryIndex])
                        : 0;
                    uint64_t durationNs = static_cast<uint64_t>(static_cast<double>(rawDelta) * m_timestampPeriod);

                    beginCpuScope("GPU::" + span.name);
                    recordDirectDuration("GPU::" + span.name, durationNs);
                    endCpuScope();
                }
            }
            endCpuScope();
        }
    }

    // ------------------------------------------------------------------------
    // CPU Hierarchical Scopes
    // ------------------------------------------------------------------------
    void beginCpuScope(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentNode = m_currentNode->getOrCreateChild(name);
        m_currentNode->callCount++;
        m_cpuTimeStack.push(std::chrono::high_resolution_clock::now());
    }

    void endCpuScope() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cpuTimeStack.empty()) return;

        auto startTime = m_cpuTimeStack.top();
        m_cpuTimeStack.pop();
        auto endTime = std::chrono::high_resolution_clock::now();

        uint64_t durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
        m_currentNode->totalDurationNs += durationNs;

        // Trace event recording
        uint64_t startUs = std::chrono::duration_cast<std::chrono::microseconds>(startTime - m_sessionStartTime).count();
        uint64_t durUs = durationNs / 1000;
        m_traceEvents.push_back({ m_currentNode->name, "cpu", 'X', startUs, durUs, 1, 1 });

        if (auto parent = m_currentNode->parent.lock()) {
            m_currentNode = parent;
        }
    }

    void recordDirectDuration(const std::string& /*name*/, uint64_t durationNs) {
        m_currentNode->totalDurationNs += durationNs;
    }

    // ------------------------------------------------------------------------
    // Export Formats: Folded Stack Format (flamegraph.pl / Speedscope)
    // ------------------------------------------------------------------------
    void exportFoldedStack(std::ostream& os, const std::shared_ptr<ProfileNode>& node, const std::string& prefix = "") {
        std::string currentPath = prefix.empty() ? node->name : (prefix + ";" + node->name);

        uint64_t childrenTotal = 0;
        for (const auto& child : node->children) {
            childrenTotal += child->totalDurationNs;
            exportFoldedStack(os, child, currentPath);
        }

        uint64_t selfTime = (node->totalDurationNs > childrenTotal) ? (node->totalDurationNs - childrenTotal) : 0;
        if (selfTime > 0 && node != m_rootNode) {
            // Output in microseconds for readability
            os << currentPath << " " << (selfTime / 1000) << "\n";
        }
    }

    bool exportFoldedFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream file(filename);
        if (!file.is_open()) return false;

        for (const auto& topChild : m_rootNode->children) {
            exportFoldedStack(file, topChild, "");
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // Export Chrome Trace Event Format (.json)
    // ------------------------------------------------------------------------
    bool exportChromeTraceFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream file(filename);
        if (!file.is_open()) return false;

        file << "{\n  \"traceEvents\": [\n";
        for (size_t i = 0; i < m_traceEvents.size(); ++i) {
            const auto& e = m_traceEvents[i];
            file << "    {\"name\": \"" << e.name << "\", \"cat\": \"" << e.cat
                 << "\", \"ph\": \"" << e.ph << "\", \"ts\": " << e.tsMicroseconds
                 << ", \"dur\": " << e.durMicroseconds << ", \"pid\": " << e.pid
                 << ", \"tid\": " << e.tid << "}";
            if (i + 1 < m_traceEvents.size()) file << ",";
            file << "\n";
        }
        file << "  ],\n  \"displayTimeUnit\": \"ms\"\n}\n";
        return true;
    }

    // ------------------------------------------------------------------------
    // Export Interactive Standalone HTML Flame Graph
    // ------------------------------------------------------------------------
    struct FlameBar {
        std::string name;
        double startPercent;
        double widthPercent;
        int depth;
        uint64_t durationUs;
        uint32_t calls;
        std::string category; // "cpu" or "gpu"
    };

    void computeLayout(const std::shared_ptr<ProfileNode>& node, int depth, double startPct, double widthPct,
                       std::vector<FlameBar>& bars) {
        if (node != m_rootNode && node->totalDurationNs > 0) {
            std::string cat = (node->name.rfind("GPU", 0) == 0) ? "gpu" : "cpu";
            bars.push_back({
                node->name,
                startPct,
                widthPct,
                depth,
                node->totalDurationNs / 1000,
                node->callCount,
                cat
            });
        }

        uint64_t totalChildDur = 0;
        for (const auto& c : node->children) totalChildDur += c->totalDurationNs;

        double currentStart = startPct;
        for (const auto& child : node->children) {
            if (totalChildDur == 0 || child->totalDurationNs == 0) continue;
            double childFraction = static_cast<double>(child->totalDurationNs) / static_cast<double>(node->totalDurationNs);
            double childWidth = widthPct * childFraction;
            computeLayout(child, (node == m_rootNode) ? 0 : (depth + 1), currentStart, childWidth, bars);
            currentStart += childWidth;
        }
    }

    bool exportInteractiveHTML(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<FlameBar> bars;

        uint64_t rootTotalNs = 0;
        for (const auto& c : m_rootNode->children) rootTotalNs += c->totalDurationNs;
        if (rootTotalNs == 0) rootTotalNs = 1;

        double currentStart = 0.0;
        for (const auto& c : m_rootNode->children) {
            double w = (static_cast<double>(c->totalDurationNs) / static_cast<double>(rootTotalNs)) * 100.0;
            computeLayout(c, 0, currentStart, w, bars);
            currentStart += w;
        }

        int maxDepth = 0;
        for (const auto& b : bars) {
            if (b.depth > maxDepth) maxDepth = b.depth;
        }

        std::ofstream html(filename);
        if (!html.is_open()) return false;

        html << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Vulkan 1.4 Flame Graph - )HTML" << m_sessionName << R"HTML(</title>
<style>
  :root {
    --bg-primary: #0d1117;
    --bg-secondary: #161b22;
    --text-primary: #e6edf3;
    --text-muted: #8b949e;
    --border: #30363d;
    --accent: #58a6ff;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace; }
  body { background: var(--bg-primary); color: var(--text-primary); padding: 24px; }
  .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; border-bottom: 1px solid var(--border); padding-bottom: 16px; }
  .header h1 { font-size: 22px; font-weight: 600; color: #f0883e; display: flex; align-items: center; gap: 8px; }
  .badge { background: #238636; color: #fff; font-size: 11px; padding: 2px 8px; border-radius: 12px; font-weight: bold; }
  .toolbar { display: flex; gap: 12px; margin-bottom: 16px; align-items: center; }
  .search-input { background: var(--bg-secondary); border: 1px solid var(--border); color: #fff; padding: 8px 14px; border-radius: 6px; width: 300px; }
  .stats-card { background: var(--bg-secondary); border: 1px solid var(--border); border-radius: 6px; padding: 8px 16px; font-size: 13px; color: var(--text-muted); }
  .graph-container { position: relative; width: 100%; height: )HTML" << ((maxDepth + 2) * 32 + 40) << R"HTML(px; background: var(--bg-secondary); border: 1px solid var(--border); border-radius: 8px; overflow: hidden; padding: 12px; }
  .flame-bar {
    position: absolute;
    height: 26px;
    border-radius: 3px;
    padding: 3px 6px;
    font-size: 11px;
    font-weight: 500;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    cursor: pointer;
    transition: transform 0.1s ease, filter 0.15s ease;
    border: 1px solid rgba(0,0,0,0.25);
    user-select: none;
  }
  .flame-bar:hover { filter: brightness(1.25); transform: translateY(-2px); z-index: 100; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }
  .cpu-bar { background: linear-gradient(135deg, #e65100, #f57c00); color: #fff; }
  .gpu-bar { background: linear-gradient(135deg, #1565c0, #0288d1); color: #fff; }
  .tooltip {
    position: fixed;
    display: none;
    background: #1f242c;
    color: #fff;
    border: 1px solid var(--accent);
    padding: 10px 14px;
    border-radius: 6px;
    font-size: 12px;
    pointer-events: none;
    z-index: 1000;
    box-shadow: 0 8px 24px rgba(0,0,0,0.6);
  }
</style>
</head>
<body>
<div class="header">
  <h1><span>&#x1F525;</span> Vulkan 1.4 Flame Graph Visualizer <span class="badge">VK 1.4 SPEC</span></h1>
  <div class="stats-card">Session: <strong>)HTML" << m_sessionName << R"HTML(</strong> | Total Samples: <strong>)HTML" << bars.size() << R"HTML(</strong></div>
</div>
<div class="toolbar">
  <input type="text" id="filterInput" class="search-input" placeholder="Search scope / function name...">
  <div class="stats-card">&#x1F7E0; Orange = CPU Scope | &#x1F535; Blue = GPU Timestamp Pass</div>
</div>
<div class="graph-container" id="container">
)HTML";

        for (const auto& b : bars) {
            int topOffset = 20 + b.depth * 30;
            std::string typeClass = (b.category == "gpu") ? "gpu-bar" : "cpu-bar";
            html << "  <div class=\"flame-bar " << typeClass << "\" style=\"left: "
                 << std::fixed << std::setprecision(3) << b.startPercent << "%; width: "
                 << b.widthPercent << "%; top: " << topOffset << "px;\" "
                 << "data-name=\"" << b.name << "\" "
                 << "data-time=\"" << b.durationUs << "\" "
                 << "data-calls=\"" << b.calls << "\" "
                 << "data-type=\"" << b.category << "\">"
                 << b.name << " (" << std::fixed << std::setprecision(2) << (b.durationUs / 1000.0) << " ms)</div>\n";
        }

        html << R"HTML(
</div>
<div id="tooltip" class="tooltip"></div>
<script>
  const tooltip = document.getElementById('tooltip');
  document.querySelectorAll('.flame-bar').forEach(bar => {
    bar.addEventListener('mousemove', (e) => {
      tooltip.style.display = 'block';
      tooltip.style.left = (e.clientX + 14) + 'px';
      tooltip.style.top = (e.clientY + 14) + 'px';
      const timeMs = (parseFloat(bar.dataset.time) / 1000).toFixed(3);
      tooltip.innerHTML = `<strong>${bar.dataset.name}</strong><br>
                           Type: ${bar.dataset.type.toUpperCase()}<br>
                           Total Time: ${timeMs} ms (${bar.dataset.time} &mu;s)<br>
                           Invocations: ${bar.dataset.calls}`;
    });
    bar.addEventListener('mouseleave', () => { tooltip.style.display = 'none'; });
  });

  const filterInput = document.getElementById('filterInput');
  filterInput.addEventListener('input', (e) => {
    const q = e.target.value.toLowerCase();
    document.querySelectorAll('.flame-bar').forEach(bar => {
      const match = bar.dataset.name.toLowerCase().includes(q);
      bar.style.opacity = (q === '' || match) ? '1' : '0.2';
      if (q !== '' && match) bar.style.outline = '2px solid #58a6ff';
      else bar.style.outline = 'none';
    });
  });
</script>
</body>
</html>
)HTML";
        return true;
    }
};

// ----------------------------------------------------------------------------
// Scoped CPU Profiler RAII Helper
// ----------------------------------------------------------------------------
class ScopedCpuProfile {
public:
    ScopedCpuProfile(const std::string& name) {
        FlameGraphProfiler::get().beginCpuScope(name);
    }
    ~ScopedCpuProfile() {
        FlameGraphProfiler::get().endCpuScope();
    }
};

// ----------------------------------------------------------------------------
// Scoped GPU Profiler RAII Helper
// ----------------------------------------------------------------------------
class ScopedGpuProfile {
    VkCommandBuffer m_cmd;
public:
    ScopedGpuProfile(VkCommandBuffer cmd, const std::string& name) : m_cmd(cmd) {
        FlameGraphProfiler::get().beginGpuScope(cmd, name);
    }
    ~ScopedGpuProfile() {
        FlameGraphProfiler::get().endGpuScope(m_cmd);
    }
};

} // namespace vk_profiler

// ----------------------------------------------------------------------------
// Convenience Profiling Macros
// ----------------------------------------------------------------------------
#define VK_PROFILE_SCOPE(name) vk_profiler::ScopedCpuProfile _cpu_prof_##__LINE__(name)
#define VK_GPU_PROFILE_SCOPE(cmd, name) vk_profiler::ScopedGpuProfile _gpu_prof_##__LINE__(cmd, name)
