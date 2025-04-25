#include "ic_visualizer.h"
#include "inline_cache.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <filesystem>

namespace aerojs {
namespace core {

//==============================================================================
// ICVisualizer クラスの実装
//==============================================================================

ICVisualizer& ICVisualizer::Instance() {
    static ICVisualizer instance;
    return instance;
}

ICVisualizer::ICVisualizer()
    : m_detailLevel(ICVisualizationDetailLevel::Basic)
    , m_format(ICVisualizationFormat::HTML)
    , m_autoRefreshInterval(5000) // 5秒ごと
    , m_cacheSizeLimit(1024 * 1024) // 1MB
    , m_lastUpdateTime(std::chrono::steady_clock::now())
    , m_isVisualizationEnabled(true)
{
    // デフォルトスタイルの設定
    m_style.fontName = "Arial";
    m_style.fontSize = 12;
    m_style.backgroundColor = "#FFFFFF";
    m_style.nodeColor = "#4285F4";
    m_style.edgeColor = "#757575";
    m_style.highlightColor = "#EA4335";
    m_style.textColor = "#000000";
    m_style.borderColor = "#DADCE0";
    m_style.borderWidth = 1;
    m_style.padding = 10;
    m_style.margin = 5;
    m_style.borderRadius = 4;
    m_style.shadowEnabled = true;
}

ICVisualizer::~ICVisualizer() {
    // キャッシュされた可視化データをクリア
    ClearCache();
    
    // ロギング
    ICLogger::Instance().Info("ICVisualizer: インスタンスを破棄しました。");
}

void ICVisualizer::SetDetailLevel(ICVisualizationDetailLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_detailLevel = level;
    ICLogger::Instance().Debug("ICVisualizer: 詳細レベルを変更しました: " + DetailLevelToString(level));
}

void ICVisualizer::SetFormat(ICVisualizationFormat format) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format = format;
    ICLogger::Instance().Debug("ICVisualizer: 出力形式を変更しました: " + FormatToString(format));
}

void ICVisualizer::SetStyle(const ICVisualizationStyle& style) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_style = style;
    ICLogger::Instance().Debug("ICVisualizer: スタイルを更新しました。");
}

void ICVisualizer::SetAutoRefreshInterval(uint32_t milliseconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoRefreshInterval = milliseconds;
    ICLogger::Instance().Debug("ICVisualizer: 自動更新間隔を設定しました: " + std::to_string(milliseconds) + "ms");
}

void ICVisualizer::SetCacheSizeLimit(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cacheSizeLimit = bytes;
    ICLogger::Instance().Debug("ICVisualizer: キャッシュサイズ制限を設定しました: " + std::to_string(bytes) + "バイト");
}

void ICVisualizer::EnableVisualization(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isVisualizationEnabled = enable;
    ICLogger::Instance().Info("ICVisualizer: 可視化機能を" + std::string(enable ? "有効" : "無効") + "にしました。");
}

bool ICVisualizer::IsVisualizationEnabled() const {
    return m_isVisualizationEnabled;
}

std::string ICVisualizer::GenerateVisualization(const std::vector<InlineCache*>& caches, ICVisualizationFormat format) {
    if (!m_isVisualizationEnabled) {
        ICLogger::Instance().Debug("ICVisualizer: 可視化機能が無効になっています。");
        return "可視化機能は現在無効になっています。";
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // キャッシュがない場合は空のデータを返す
    if (caches.empty()) {
        ICLogger::Instance().Debug("ICVisualizer: 可視化するキャッシュがありません。");
        return "可視化するキャッシュがありません。";
    }

    // フォーマットが指定されていれば、それを使用
    ICVisualizationFormat targetFormat = (format != ICVisualizationFormat::DOT) ? format : m_format;
    
    // キャッシュキーの生成
    std::string cacheKey = GenerateCacheKey(caches, targetFormat);
    
    // キャッシュにヒットした場合は、キャッシュから返す
    auto it = m_visualizationCache.find(cacheKey);
    if (it != m_visualizationCache.end()) {
        // キャッシュされたデータが古くなっていないか確認
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTime).count() < m_autoRefreshInterval) {
            ICLogger::Instance().Debug("ICVisualizer: キャッシュから可視化データを取得しました。");
            return it->second;
        }
    }

    // 最終更新時間を更新
    m_lastUpdateTime = std::chrono::steady_clock::now();
    
    // 新しい可視化データを生成
    std::string visualizationData;
    
    switch (targetFormat) {
        case ICVisualizationFormat::DOT:
            visualizationData = GenerateDOTVisualization(caches);
            break;
        case ICVisualizationFormat::JSON:
            visualizationData = GenerateJSONVisualization(caches);
            break;
        case ICVisualizationFormat::HTML:
            visualizationData = GenerateHTMLVisualization(caches);
            break;
        case ICVisualizationFormat::SVG:
            visualizationData = GenerateSVGVisualization(caches);
            break;
        case ICVisualizationFormat::TXT:
            visualizationData = GenerateTextVisualization(caches);
            break;
        case ICVisualizationFormat::CSV:
            visualizationData = GenerateCSVVisualization(caches);
            break;
        case ICVisualizationFormat::XML:
            visualizationData = GenerateXMLVisualization(caches);
            break;
        default:
            ICLogger::Instance().Error("ICVisualizer: サポートされていない出力形式です。");
            return "サポートされていない出力形式です。";
    }

    // キャッシュに保存
    m_visualizationCache[cacheKey] = visualizationData;
    
    // キャッシュサイズの管理
    ManageCacheSize();
    
    ICLogger::Instance().Debug("ICVisualizer: 新しい可視化データを生成しました。");
    return visualizationData;
}

std::string ICVisualizer::SaveVisualizationToFile(const std::vector<InlineCache*>& caches, 
                                                const std::string& filePath, 
                                                ICVisualizationFormat format) {
    if (!m_isVisualizationEnabled) {
        ICLogger::Instance().Debug("ICVisualizer: 可視化機能が無効になっています。");
        return "";
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 保存先ディレクトリの確認
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());
    
    // 可視化データの生成
    std::string visualizationData = GenerateVisualization(caches, format);
    
    // ファイルへの書き込み
    try {
        std::ofstream file(filePath, std::ios::trunc);
        if (!file.is_open()) {
            ICLogger::Instance().Error("ICVisualizer: ファイルを開けませんでした: " + filePath);
            return "";
        }
        
        file << visualizationData;
        file.close();
        
        ICLogger::Instance().Info("ICVisualizer: 可視化データをファイルに保存しました: " + filePath);
        return filePath;
    } catch (const std::exception& e) {
        ICLogger::Instance().Error("ICVisualizer: ファイル保存中にエラーが発生しました: " + std::string(e.what()));
        return "";
    }
}

void ICVisualizer::ClearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_visualizationCache.clear();
    ICLogger::Instance().Debug("ICVisualizer: 可視化キャッシュをクリアしました。");
}

std::string ICVisualizer::GenerateCacheKey(const std::vector<InlineCache*>& caches, ICVisualizationFormat format) {
    std::stringstream ss;
    
    // フォーマットを追加
    ss << "format:" << static_cast<int>(format) << "_";
    
    // 詳細レベルを追加
    ss << "level:" << static_cast<int>(m_detailLevel) << "_";
    
    // 各キャッシュのアドレスとバージョンを追加
    for (const auto& cache : caches) {
        if (cache) {
            ss << reinterpret_cast<uintptr_t>(cache) << "_";
            ss << cache->GetVersion() << "_";
        }
    }
    
    return ss.str();
}

void ICVisualizer::ManageCacheSize() {
    // キャッシュのサイズを計算
    size_t totalSize = 0;
    for (const auto& entry : m_visualizationCache) {
        totalSize += entry.first.size() + entry.second.size();
    }
    
    // キャッシュサイズが制限を超えた場合、古いエントリから削除
    if (totalSize > m_cacheSizeLimit && !m_visualizationCache.empty()) {
        // 最も古いエントリを削除
        auto oldestEntry = m_visualizationCache.begin();
        m_visualizationCache.erase(oldestEntry);
        
        ICLogger::Instance().Debug("ICVisualizer: キャッシュサイズ制限に達したため、古いエントリを削除しました。");
        
        // 再帰的に呼び出して、必要なだけ削除
        ManageCacheSize();
    }
}

std::string ICVisualizer::GenerateDOTVisualization(const std::vector<InlineCache*>& caches) {
    std::stringstream ss;
    
    // DOT形式のヘッダー
    ss << "digraph InlineCaches {\n";
    ss << "  graph [fontname=\"" << m_style.fontName << "\", fontsize=" << m_style.fontSize;
    ss << ", bgcolor=\"" << m_style.backgroundColor << "\"];\n";
    ss << "  node [fontname=\"" << m_style.fontName << "\", fontsize=" << m_style.fontSize;
    ss << ", style=filled, fillcolor=\"" << m_style.nodeColor << "\", color=\"" << m_style.borderColor;
    ss << "\", shape=box];\n";
    ss << "  edge [fontname=\"" << m_style.fontName << "\", fontsize=" << m_style.fontSize;
    ss << ", color=\"" << m_style.edgeColor << "\"];\n\n";
    
    // ノードとエッジの定義
    int nodeId = 0;
    std::unordered_map<const InlineCache*, int> cacheNodeIds;
    
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        int cacheId = nodeId++;
        cacheNodeIds[cache] = cacheId;
        
        // キャッシュノードの定義
        ss << "  node" << cacheId << " [label=\"";
        
        // ノードのラベル内容（詳細レベルに応じて）
        if (m_detailLevel >= ICVisualizationDetailLevel::Basic) {
            ss << "ID: " << cache->GetId() << "\\n";
            ss << "タイプ: " << ICTypeToString(cache->GetType()) << "\\n";
        }
        
        if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
            const auto& stats = cache->GetStatistics();
            ss << "ヒット: " << stats.hits << "\\n";
            ss << "ミス: " << stats.misses << "\\n";
            
            // ヒット率の計算
            uint64_t total = stats.hits + stats.misses;
            double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
            ss << "ヒット率: " << std::fixed << std::setprecision(2) << hitRate << "%\\n";
        }
        
        if (m_detailLevel >= ICVisualizationDetailLevel::Complete) {
            ss << "バージョン: " << cache->GetVersion() << "\\n";
            ss << "エントリ数: " << cache->GetEntryCount() << "\\n";
            ss << "最大エントリ数: " << cache->GetMaxEntries() << "\\n";
            ss << "無効化回数: " << cache->GetStatistics().invalidations << "\\n";
        }
        
        ss << "\", ";
        
        // ヒット率に基づいてノードの色を調整
        if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
            const auto& stats = cache->GetStatistics();
            uint64_t total = stats.hits + stats.misses;
            double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) : 0.0;
            
            // ヒット率に応じた色のグラデーション（緑→黄→赤）
            std::string color;
            if (hitRate >= 0.8) {
                color = "#4CAF50"; // 緑（良好）
            } else if (hitRate >= 0.5) {
                color = "#FFC107"; // 黄（注意）
            } else {
                color = "#F44336"; // 赤（警告）
            }
            
            ss << "fillcolor=\"" << color << "\"";
        }
        
        ss << "];\n";
        
        // エントリを表現するサブノードとエッジ
        if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
            const auto& entries = cache->GetEntries();
            for (size_t i = 0; i < entries.size(); ++i) {
                int entryId = nodeId++;
                
                ss << "  node" << entryId << " [label=\"";
                ss << "エントリ " << i << "\\n";
                
                if (m_detailLevel >= ICVisualizationDetailLevel::Complete) {
                    ss << "キー: " << entries[i].key << "\\n";
                    ss << "アクセス回数: " << entries[i].accessCount << "\\n";
                    ss << "最終アクセス: " << FormatTimestamp(entries[i].lastAccessTime) << "\\n";
                }
                
                ss << "\", shape=ellipse, fillcolor=\"#81C784\"];\n";
                
                // キャッシュからエントリへのエッジ
                ss << "  node" << cacheId << " -> node" << entryId << ";\n";
            }
        }
    }
    
    // 依存関係のあるキャッシュ間のエッジ
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& dependencies = cache->GetDependencies();
        for (const auto& dep : dependencies) {
            if (cacheNodeIds.find(dep) != cacheNodeIds.end()) {
                ss << "  node" << cacheNodeIds[cache] << " -> node" << cacheNodeIds[dep] 
                   << " [style=dashed, label=\"依存\"];\n";
            }
        }
    }
    
    // DOT形式のフッター
    ss << "}\n";
    
    return ss.str();
}

std::string ICVisualizer::GenerateJSONVisualization(const std::vector<InlineCache*>& caches) {
    std::stringstream ss;
    
    // JSONヘッダー
    ss << "{\n";
    ss << "  \"timestamp\": \"" << EscapeJSONString(GetCurrentTimestamp()) << "\",\n";
    
    // パフォーマンスサマリー
    ICPerformanceSummary summary = AnalyzePerformance(caches);
    ss << "  \"performanceSummary\": {\n";
    ss << "    \"overallHitRate\": " << std::fixed << std::setprecision(2) << summary.overallHitRate << ",\n";
    ss << "    \"totalHits\": " << summary.totalHits << ",\n";
    ss << "    \"totalMisses\": " << summary.totalMisses << ",\n";
    ss << "    \"totalInvalidations\": " << summary.totalInvalidations << ",\n";
    ss << "    \"efficiencyScore\": " << std::fixed << std::setprecision(2) << summary.efficiencyScore << ",\n";
    ss << "    \"recommendation\": \"" << EscapeJSONString(summary.recommendation) << "\"\n";
    ss << "  },\n";
    
    // 各キャッシュの情報
    ss << "  \"caches\": [\n";
    
    bool firstCache = true;
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        if (!firstCache) {
            ss << ",\n";
        }
        
        const auto& stats = cache->GetStatistics();
        uint64_t total = stats.hits + stats.misses;
        double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
        
        ss << "    {\n";
        ss << "      \"id\": " << cache->GetId() << ",\n";
        ss << "      \"type\": \"" << EscapeJSONString(ICTypeToString(cache->GetType())) << "\",\n";
        ss << "      \"version\": " << cache->GetVersion() << ",\n";
        ss << "      \"entryCount\": " << cache->GetEntryCount() << ",\n";
        ss << "      \"maxEntries\": " << cache->GetMaxEntries() << ",\n";
        
        // 統計情報
        ss << "      \"statistics\": {\n";
        ss << "        \"hits\": " << stats.hits << ",\n";
        ss << "        \"misses\": " << stats.misses << ",\n";
        ss << "        \"hitRate\": " << std::fixed << std::setprecision(2) << hitRate << ",\n";
        ss << "        \"invalidations\": " << stats.invalidations << "\n";
        ss << "      }";
        
        // エントリ情報（詳細レベルに応じて）
        if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
            const auto& entries = cache->GetEntries();
            ss << ",\n      \"entries\": [\n";
            
            bool firstEntry = true;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (!firstEntry) {
                    ss << ",\n";
                }
                
                ss << "        {\n";
                ss << "          \"index\": " << i << ",\n";
                ss << "          \"key\": \"" << EscapeJSONString(entries[i].key) << "\",\n";
                ss << "          \"accessCount\": " << entries[i].accessCount << ",\n";
                ss << "          \"lastAccess\": \"" << EscapeJSONString(FormatTimestamp(entries[i].lastAccessTime)) << "\"\n";
                ss << "        }";
                
                firstEntry = false;
            }
            
            ss << "\n      ]";
        }
        
        // 依存関係
        const auto& dependencies = cache->GetDependencies();
        ss << ",\n      \"dependencies\": [";
        
        bool firstDep = true;
        for (const auto& dep : dependencies) {
            if (!firstDep) {
                ss << ", ";
            }
            
            ss << dep->GetId();
            firstDep = false;
        }
        
        ss << "]\n";
        ss << "    }";
        
        firstCache = false;
    }
    
    ss << "\n  ]\n";
    ss << "}\n";
    
    return ss.str();
}

std::string ICVisualizer::GenerateHTMLVisualization(const std::vector<InlineCache*>& caches) {
    std::stringstream ss;
    
    // HTMLヘッダー
    ss << "<!DOCTYPE html>\n";
    ss << "<html lang=\"ja\">\n";
    ss << "<head>\n";
    ss << "  <meta charset=\"UTF-8\">\n";
    ss << "  <title>インラインキャッシュ可視化</title>\n";
    ss << "  <style>\n";
    ss << "    body { font-family: Arial, sans-serif; margin: 20px; }\n";
    ss << "    h1, h2 { color: #333; }\n";
    ss << "    .cache { margin-bottom: 30px; border: 1px solid #ddd; padding: 15px; border-radius: 5px; }\n";
    ss << "    .cache-header { background-color: #f5f5f5; padding: 10px; margin-bottom: 10px; }\n";
    ss << "    .stats { display: flex; flex-wrap: wrap; margin-bottom: 15px; }\n";
    ss << "    .stat-item { margin-right: 20px; margin-bottom: 10px; }\n";
    ss << "    .stat-label { font-weight: bold; color: #555; }\n";
    ss << "    .good { color: green; }\n";
    ss << "    .warning { color: orange; }\n";
    ss << "    .poor { color: red; }\n";
    ss << "    table { border-collapse: collapse; width: 100%; margin-top: 10px; }\n";
    ss << "    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    ss << "    th { background-color: #f2f2f2; }\n";
    ss << "    tr:nth-child(even) { background-color: #f9f9f9; }\n";
    ss << "    .chart-container { width: 100%; height: 200px; margin: 20px 0; }\n";
    ss << "  </style>\n";
    
    // 詳細レベルが非常に高い場合はビジュアルグラフ用のライブラリを追加
    if (m_detailLevel >= ICVisualizationDetailLevel::VeryDetailed) {
        ss << "  <script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n";
    }
    
    ss << "</head>\n";
    ss << "<body>\n";
    ss << "  <h1>インラインキャッシュ可視化</h1>\n";
    ss << "  <p>生成日時: " << GetCurrentTimestamp() << "</p>\n";
    
    // パフォーマンスサマリー
    ICPerformanceSummary summary = AnalyzePerformance(caches);
    ss << "  <h2>パフォーマンスサマリー</h2>\n";
    ss << "  <div class=\"stats\">\n";
    ss << "    <div class=\"stat-item\">\n";
    ss << "      <div class=\"stat-label\">総合キャッシュヒット率:</div>\n";
    
    // ヒット率に基づいた色分け
    std::string hitRateClass = "good";
    if (summary.overallHitRate < 80.0) {
        hitRateClass = "warning";
    }
    if (summary.overallHitRate < 50.0) {
        hitRateClass = "poor";
    }
    
    ss << "      <div class=\"" << hitRateClass << "\">" << std::fixed << std::setprecision(2) << summary.overallHitRate << "%</div>\n";
    ss << "    </div>\n";
    ss << "    <div class=\"stat-item\">\n";
    ss << "      <div class=\"stat-label\">キャッシュ総数:</div>\n";
    ss << "      <div>" << caches.size() << "</div>\n";
    ss << "    </div>\n";
    ss << "    <div class=\"stat-item\">\n";
    ss << "      <div class=\"stat-label\">合計ヒット数:</div>\n";
    ss << "      <div>" << summary.totalHits << "</div>\n";
    ss << "    </div>\n";
    ss << "    <div class=\"stat-item\">\n";
    ss << "      <div class=\"stat-label\">合計ミス数:</div>\n";
    ss << "      <div>" << summary.totalMisses << "</div>\n";
    ss << "    </div>\n";
    ss << "  </div>\n";
    
    // 詳細レベルが非常に高い場合はグラフを追加
    if (m_detailLevel >= ICVisualizationDetailLevel::VeryDetailed) {
        ss << "  <div class=\"chart-container\">\n";
        ss << "    <canvas id=\"hitRateChart\"></canvas>\n";
        ss << "  </div>\n";
        
        // グラフ用のJavaScriptコード
        ss << "  <script>\n";
        ss << "    document.addEventListener('DOMContentLoaded', function() {\n";
        ss << "      const ctx = document.getElementById('hitRateChart').getContext('2d');\n";
        ss << "      const labels = [";
        
        bool first = true;
        for (const auto& cache : caches) {
            if (!cache) continue;
            
            if (!first) {
                ss << ", ";
            }
            ss << "'" << EscapeJSONString(ICTypeToString(cache->GetType())) << " " << cache->GetId() << "'";
            first = false;
        }
        
        ss << "];\n";
        ss << "      const hitRates = [";
        
        first = true;
        for (const auto& cache : caches) {
            if (!cache) continue;
            
            if (!first) {
                ss << ", ";
            }
            
            const auto& stats = cache->GetStatistics();
            uint64_t total = stats.hits + stats.misses;
            double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
            
            ss << std::fixed << std::setprecision(2) << hitRate;
            first = false;
        }
        
        ss << "];\n";
        
        ss << "      new Chart(ctx, {\n";
        ss << "        type: 'bar',\n";
        ss << "        data: {\n";
        ss << "          labels: labels,\n";
        ss << "          datasets: [{\n";
        ss << "            label: 'ヒット率 (%)',\n";
        ss << "            data: hitRates,\n";
        ss << "            backgroundColor: hitRates.map(rate => {\n";
        ss << "              if (rate >= 80) return 'rgba(75, 192, 192, 0.6)';\n";
        ss << "              if (rate >= 50) return 'rgba(255, 159, 64, 0.6)';\n";
        ss << "              return 'rgba(255, 99, 132, 0.6)';\n";
        ss << "            }),\n";
        ss << "            borderColor: hitRates.map(rate => {\n";
        ss << "              if (rate >= 80) return 'rgba(75, 192, 192, 1)';\n";
        ss << "              if (rate >= 50) return 'rgba(255, 159, 64, 1)';\n";
        ss << "              return 'rgba(255, 99, 132, 1)';\n";
        ss << "            }),\n";
        ss << "            borderWidth: 1\n";
        ss << "          }]\n";
        ss << "        },\n";
        ss << "        options: {\n";
        ss << "          scales: {\n";
        ss << "            y: {\n";
        ss << "              beginAtZero: true,\n";
        ss << "              max: 100\n";
        ss << "            }\n";
        ss << "          },\n";
        ss << "          plugins: {\n";
        ss << "            title: {\n";
        ss << "              display: true,\n";
        ss << "              text: 'キャッシュ別ヒット率'\n";
        ss << "            }\n";
        ss << "          }\n";
        ss << "        }\n";
        ss << "      });\n";
        ss << "    });\n";
        ss << "  </script>\n";
    }
    
    // 各キャッシュの詳細
    ss << "  <h2>キャッシュ詳細</h2>\n";
    
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& stats = cache->GetStatistics();
        uint64_t total = stats.hits + stats.misses;
        double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
        
        // ヒット率に基づいた色分け
        std::string hitRateClass = "good";
        if (hitRate < 80.0) {
            hitRateClass = "warning";
        }
        if (hitRate < 50.0) {
            hitRateClass = "poor";
        }
        
        ss << "  <div class=\"cache\">\n";
        ss << "    <div class=\"cache-header\">\n";
        ss << "      <h3>キャッシュ #" << cache->GetId() << " - " << EscapeHTMLString(ICTypeToString(cache->GetType())) << "</h3>\n";
        ss << "    </div>\n";
        
        ss << "    <div class=\"stats\">\n";
        ss << "      <div class=\"stat-item\">\n";
        ss << "        <div class=\"stat-label\">バージョン:</div>\n";
        ss << "        <div>" << cache->GetVersion() << "</div>\n";
        ss << "      </div>\n";
        ss << "      <div class=\"stat-item\">\n";
        ss << "        <div class=\"stat-label\">エントリ数:</div>\n";
        ss << "        <div>" << cache->GetEntryCount() << " / " << cache->GetMaxEntries() << "</div>\n";
        ss << "      </div>\n";
        ss << "      <div class=\"stat-item\">\n";
        ss << "        <div class=\"stat-label\">ヒット数:</div>\n";
        ss << "        <div>" << stats.hits << "</div>\n";
        ss << "      </div>\n";
        ss << "      <div class=\"stat-item\">\n";
        ss << "        <div class=\"stat-label\">ミス数:</div>\n";
        ss << "        <div>" << stats.misses << "</div>\n";
        ss << "      </div>\n";
        ss << "      <div class=\"stat-item\">\n";
        ss << "        <div class=\"stat-label\">ヒット率:</div>\n";
        ss << "        <div class=\"" << hitRateClass << "\">" << std::fixed << std::setprecision(2) << hitRate << "%</div>\n";
        ss << "      </div>\n";
        ss << "      <div class=\"stat-item\">\n";
        ss << "        <div class=\"stat-label\">無効化回数:</div>\n";
        ss << "        <div>" << stats.invalidations << "</div>\n";
        ss << "      </div>\n";
        ss << "    </div>\n";
        
        // 詳細レベルに応じてエントリ情報を表示
        if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
            const auto& entries = cache->GetEntries();
            
            ss << "    <h4>エントリ一覧 (" << entries.size() << "件)</h4>\n";
            
            if (!entries.empty()) {
                ss << "    <table>\n";
                ss << "      <tr>\n";
                ss << "        <th>#</th>\n";
                ss << "        <th>キー</th>\n";
                ss << "        <th>アクセス数</th>\n";
                ss << "        <th>最終アクセス</th>\n";
                ss << "      </tr>\n";
                
                for (size_t i = 0; i < entries.size(); ++i) {
                    ss << "      <tr>\n";
                    ss << "        <td>" << i << "</td>\n";
                    ss << "        <td>" << EscapeHTMLString(entries[i].key) << "</td>\n";
                    ss << "        <td>" << entries[i].accessCount << "</td>\n";
                    ss << "        <td>" << EscapeHTMLString(FormatTimestamp(entries[i].lastAccessTime)) << "</td>\n";
                    ss << "      </tr>\n";
                }
                
                ss << "    </table>\n";
            } else {
                ss << "    <p>エントリなし</p>\n";
            }
        }
        
        // 依存関係
        const auto& dependencies = cache->GetDependencies();
        ss << "    <h4>依存関係 (" << dependencies.size() << "件)</h4>\n";
        
        if (!dependencies.empty()) {
            ss << "    <ul>\n";
            
            for (const auto& dep : dependencies) {
                ss << "      <li>キャッシュ #" << dep->GetId() << " - " << EscapeHTMLString(ICTypeToString(dep->GetType())) << "</li>\n";
            }
            
            ss << "    </ul>\n";
        } else {
            ss << "    <p>依存なし</p>\n";
        }
        
        ss << "  </div>\n";
    }
    
    // HTMLフッター
    ss << "</body>\n";
    ss << "</html>\n";
    
    return ss.str();
}

std::string ICVisualizer::GenerateCSVVisualization(const std::vector<InlineCache*>& caches) {
    std::stringstream ss;
    
    // ヘッダー行
    ss << "キャッシュID,タイプ,バージョン,エントリ数,最大エントリ数,ヒット数,ミス数,ヒット率,無効化回数\n";
    
    // 各キャッシュ行
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& stats = cache->GetStatistics();
        uint64_t total = stats.hits + stats.misses;
        double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
        
        ss << cache->GetId() << ","
           << EscapeCSVString(ICTypeToString(cache->GetType())) << ","
           << cache->GetVersion() << ","
           << cache->GetEntryCount() << ","
           << cache->GetMaxEntries() << ","
           << stats.hits << ","
           << stats.misses << ","
           << std::fixed << std::setprecision(2) << hitRate << ","
           << stats.invalidations << "\n";
    }
    
    // 詳細レベルが高い場合は各エントリの情報も追加
    if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
        ss << "\n\nキャッシュID,エントリインデックス,キー,アクセス数,最終アクセス\n";
        
        for (const auto& cache : caches) {
            if (!cache) continue;
            
            const auto& entries = cache->GetEntries();
            for (size_t i = 0; i < entries.size(); ++i) {
                ss << cache->GetId() << ","
                   << i << ","
                   << EscapeCSVString(entries[i].key) << ","
                   << entries[i].accessCount << ","
                   << EscapeCSVString(FormatTimestamp(entries[i].lastAccessTime)) << "\n";
            }
        }
    }
    
    return ss.str();
}

std::string ICVisualizer::GenerateXMLVisualization(const std::vector<InlineCache*>& caches) {
    std::stringstream ss;
    
    // XMLヘッダーとルート要素
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<InlineCacheVisualization timestamp=\"" << EscapeXMLString(GetCurrentTimestamp()) << "\">\n";
    
    // パフォーマンスサマリー
    ICPerformanceSummary summary = AnalyzePerformance(caches);
    ss << "  <PerformanceSummary>\n";
    ss << "    <OverallHitRate>" << std::fixed << std::setprecision(2) << summary.overallHitRate << "</OverallHitRate>\n";
    ss << "    <TotalHits>" << summary.totalHits << "</TotalHits>\n";
    ss << "    <TotalMisses>" << summary.totalMisses << "</TotalMisses>\n";
    ss << "    <TotalInvalidations>" << summary.totalInvalidations << "</TotalInvalidations>\n";
    ss << "    <EfficiencyScore>" << std::fixed << std::setprecision(2) << summary.efficiencyScore << "</EfficiencyScore>\n";
    ss << "    <Recommendation>" << EscapeXMLString(summary.recommendation) << "</Recommendation>\n";
    ss << "  </PerformanceSummary>\n";
    
    // 各キャッシュの情報
    ss << "  <Caches>\n";
    
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& stats = cache->GetStatistics();
        uint64_t total = stats.hits + stats.misses;
        double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
        
        ss << "    <Cache id=\"" << cache->GetId() << "\">\n";
        ss << "      <Type>" << EscapeXMLString(ICTypeToString(cache->GetType())) << "</Type>\n";
        ss << "      <Version>" << cache->GetVersion() << "</Version>\n";
        ss << "      <EntryCount>" << cache->GetEntryCount() << "</EntryCount>\n";
        ss << "      <MaxEntries>" << cache->GetMaxEntries() << "</MaxEntries>\n";
        
        // 統計情報
        ss << "      <Statistics>\n";
        ss << "        <Hits>" << stats.hits << "</Hits>\n";
        ss << "        <Misses>" << stats.misses << "</Misses>\n";
        ss << "        <HitRate>" << std::fixed << std::setprecision(2) << hitRate << "</HitRate>\n";
        ss << "        <Invalidations>" << stats.invalidations << "</Invalidations>\n";
        ss << "      </Statistics>\n";
        
        // エントリ情報（詳細レベルに応じて）
        if (m_detailLevel >= ICVisualizationDetailLevel::Detailed) {
            const auto& entries = cache->GetEntries();
            ss << "      <Entries count=\"" << entries.size() << "\">\n";
            
            for (size_t i = 0; i < entries.size(); ++i) {
                ss << "        <Entry index=\"" << i << "\">\n";
                ss << "          <Key>" << EscapeXMLString(entries[i].key) << "</Key>\n";
                ss << "          <AccessCount>" << entries[i].accessCount << "</AccessCount>\n";
                ss << "          <LastAccess>" << EscapeXMLString(FormatTimestamp(entries[i].lastAccessTime)) << "</LastAccess>\n";
                ss << "        </Entry>\n";
            }
            
            ss << "      </Entries>\n";
        }
        
        // 依存関係
        const auto& dependencies = cache->GetDependencies();
        ss << "      <Dependencies count=\"" << dependencies.size() << "\">\n";
        
        for (const auto& dep : dependencies) {
            ss << "        <DependsOn id=\"" << dep->GetId() << "\" type=\""
               << EscapeXMLString(ICTypeToString(dep->GetType())) << "\" />\n";
        }
        
        ss << "      </Dependencies>\n";
        ss << "    </Cache>\n";
    }
    
    ss << "  </Caches>\n";
    ss << "</InlineCacheVisualization>\n";
    
    return ss.str();
}

// ユーティリティ関数
std::string ICVisualizer::GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ICVisualizer::FormatTimestamp(uint64_t timestamp) const {
    if (timestamp == 0) {
        return "なし";
    }
    
    auto time = static_cast<time_t>(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ICVisualizer::EscapeHTMLString(const std::string& str) const {
    std::stringstream ss;
    
    for (char c : str) {
        switch (c) {
            case '&': ss << "&amp;"; break;
            case '<': ss << "&lt;"; break;
            case '>': ss << "&gt;"; break;
            case '"': ss << "&quot;"; break;
            case '\'': ss << "&#39;"; break;
            default: ss << c;
        }
    }
    
    return ss.str();
}

std::string ICVisualizer::EscapeJSONString(const std::string& str) const {
    std::stringstream ss;
    
    for (char c : str) {
        switch (c) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    
    return ss.str();
}

std::string ICVisualizer::EscapeXMLString(const std::string& str) const {
    std::stringstream ss;
    
    for (char c : str) {
        switch (c) {
            case '&': ss << "&amp;"; break;
            case '<': ss << "&lt;"; break;
            case '>': ss << "&gt;"; break;
            case '"': ss << "&quot;"; break;
            case '\'': ss << "&apos;"; break;
            default: ss << c;
        }
    }
    
    return ss.str();
}

std::string ICVisualizer::EscapeCSVString(const std::string& str) const {
    // CSVでは、ダブルクォートで囲み、内部のダブルクォートはダブルクォートでエスケープする
    if (str.find(',') == std::string::npos && 
        str.find('"') == std::string::npos && 
        str.find('\n') == std::string::npos) {
        return str;
    }
    
    std::stringstream ss;
    ss << '"';
    
    for (char c : str) {
        if (c == '"') {
            ss << "\"\"";
        } else {
            ss << c;
        }
    }
    
    ss << '"';
    return ss.str();
}

// パフォーマンス分析ユーティリティ
ICPerformanceSummary ICVisualizer::AnalyzePerformance(const std::vector<InlineCache*>& caches) const {
    ICPerformanceSummary summary;
    summary.totalHits = 0;
    summary.totalMisses = 0;
    summary.totalInvalidations = 0;
    summary.overallHitRate = 0.0;
    summary.efficiencyScore = 0.0;
    summary.recommendation = "";
    
    uint64_t totalCacheSizes = 0;
    
    // 基本的な統計情報の集計
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& stats = cache->GetStatistics();
        summary.totalHits += stats.hits;
        summary.totalMisses += stats.misses;
        summary.totalInvalidations += stats.invalidations;
        totalCacheSizes += cache->GetMaxEntries();
    }
    
    // ヒット率計算
    uint64_t totalAccesses = summary.totalHits + summary.totalMisses;
    if (totalAccesses > 0) {
        summary.overallHitRate = (static_cast<double>(summary.totalHits) / totalAccesses) * 100.0;
    }
    
    // 効率スコア計算
    if (totalCacheSizes > 0) {
        // 基本スコアはヒット率
        double baseScore = summary.overallHitRate;
        
        // 無効化が多いほどスコアを下げる
        double invalidationPenalty = 0.0;
        if (totalAccesses > 0) {
            invalidationPenalty = (static_cast<double>(summary.totalInvalidations) / totalAccesses) * 20.0;
            invalidationPenalty = std::min(invalidationPenalty, 20.0);
        }
        
        // 最終スコア計算
        summary.efficiencyScore = baseScore - invalidationPenalty;
        summary.efficiencyScore = std::max(summary.efficiencyScore, 0.0);
    }
    
    // 推奨事項の生成
    if (summary.overallHitRate < 50.0) {
        summary.recommendation = "ヒット率が低すぎます。キャッシュサイズの拡大やキャッシュポリシーの見直しを検討してください。";
    } else if (summary.overallHitRate < 80.0) {
        summary.recommendation = "ヒット率は中程度です。特定のキャッシュの改善やフィードバック機構の実装を検討してください。";
    } else if (summary.totalInvalidations > totalAccesses * 0.1) {
        summary.recommendation = "ヒット率は良好ですが、無効化の頻度が高すぎます。依存関係を見直し、無効化の条件を最適化してください。";
    } else {
        summary.recommendation = "パフォーマンスは良好です。現在の設定を維持してください。";
    }
    
    return summary;
}

// ICTypeをstring表現に変換
std::string ICVisualizer::ICTypeToString(ICType type) const {
    switch (type) {
        case ICType::Property: return "プロパティ";
        case ICType::Method: return "メソッド";
        case ICType::Constructor: return "コンストラクタ";
        case ICType::Prototype: return "プロトタイプ";
        case ICType::GlobalLookup: return "グローバル検索";
        case ICType::BinaryOperation: return "二項演算";
        case ICType::UnaryOperation: return "単項演算";
        case ICType::Comparison: return "比較演算";
        default: return "不明";
    }
}

// 詳細レベルの文字列表現を取得
std::string ICVisualizer::DetailLevelToString(ICVisualizationDetailLevel level) const {
    switch (level) {
        case ICVisualizationDetailLevel::Basic: return "基本";
        case ICVisualizationDetailLevel::Detailed: return "詳細";
        case ICVisualizationDetailLevel::VeryDetailed: return "非常に詳細";
        default: return "不明";
    }
}

// 出力形式の文字列表現を取得
std::string ICVisualizer::FormatToString(ICVisualizationFormat format) const {
    switch (format) {
        case ICVisualizationFormat::Text: return "テキスト";
        case ICVisualizationFormat::HTML: return "HTML";
        case ICVisualizationFormat::JSON: return "JSON";
        case ICVisualizationFormat::XML: return "XML";
        case ICVisualizationFormat::CSV: return "CSV";
        default: return "不明";
    }
}

// 抽象キャッシュエントリクラス
class ICVisualizerCacheEntry {
public:
    ICVisualizerCacheEntry(const std::string& key, uint64_t accessCount, uint64_t lastAccessTime)
        : key(key), accessCount(accessCount), lastAccessTime(lastAccessTime) {}
    
    std::string key;
    uint64_t accessCount;
    uint64_t lastAccessTime;
};

// キャッシュグラフの生成
void ICVisualizer::GenerateCacheGraph(const std::vector<InlineCache*>& caches, const std::string& outputPath) const {
    if (caches.empty()) {
        return;
    }
    
    std::ofstream dotFile(outputPath + ".dot");
    if (!dotFile) {
        LOG_ERROR("キャッシュグラフDOTファイルを作成できませんでした: %s", (outputPath + ".dot").c_str());
        return;
    }
    
    // DOTファイルのヘッダー
    dotFile << "digraph ICGraph {\n";
    dotFile << "  graph [rankdir=LR, fontname=\"Arial\", fontsize=12];\n";
    dotFile << "  node [shape=box, style=filled, fontname=\"Arial\", fontsize=10];\n";
    dotFile << "  edge [fontname=\"Arial\", fontsize=8];\n";
    
    // ノード定義
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& stats = cache->GetStatistics();
        uint64_t total = stats.hits + stats.misses;
        double hitRate = (total > 0) ? (static_cast<double>(stats.hits) / total) * 100.0 : 0.0;
        
        std::string color;
        if (hitRate >= 80.0) {
            color = "\"#a3d977\""; // 緑 - 良好
        } else if (hitRate >= 50.0) {
            color = "\"#ffe066\""; // 黄色 - 中程度
        } else {
            color = "\"#ff9966\""; // 赤 - 不良
        }
        
        std::stringstream label;
        label << "Cache #" << cache->GetId() << "\\n"
              << EscapeJSONString(ICTypeToString(cache->GetType())) << "\\n"
              << "Hit Rate: " << std::fixed << std::setprecision(1) << hitRate << "%\\n"
              << "Entries: " << cache->GetEntryCount() << "/" << cache->GetMaxEntries();
        
        dotFile << "  cache" << cache->GetId() << " [label=\"" << label.str() 
                << "\", fillcolor=" << color << "];\n";
    }
    
    // エッジ定義（依存関係）
    for (const auto& cache : caches) {
        if (!cache) continue;
        
        const auto& dependencies = cache->GetDependencies();
        for (const auto& dep : dependencies) {
            dotFile << "  cache" << cache->GetId() << " -> cache" << dep->GetId() << ";\n";
        }
    }
    
    dotFile << "}\n";
    dotFile.close();
    
    // GraphvizでDOTファイルからPNGを生成
    std::string command = "dot -Tpng -o \"" + outputPath + ".png\" \"" + outputPath + ".dot\"";
    int result = system(command.c_str());
    
    if (result != 0) {
        LOG_ERROR("Graphvizでグラフ画像を生成できませんでした。Graphvizがインストールされているか確認してください。");
    } else {
        LOG_INFO("キャッシュグラフを生成しました: %s", (outputPath + ".png").c_str());
    }
}
}
} 