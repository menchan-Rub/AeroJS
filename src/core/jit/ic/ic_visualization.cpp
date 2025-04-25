#include "ic_visualization.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <regex>
#include <optional>
#include <numeric>

namespace aerojs {
namespace core {

//==============================================================================
// ユーティリティ関数
//==============================================================================

namespace {

// 現在時刻をISO 8601形式で取得する
std::string GetISOTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    
    return ss.str();
}

// ランダムなIDを生成する
std::string GenerateRandomId(size_t length = 16) {
    static const char charset[] = 
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    
    return result;
}

// HTMLをエスケープする
std::string EscapeHTML(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    
    for (auto c : input) {
        switch (c) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += "&quot;"; break;
            case '\'': output += "&#39;"; break;
            default: output += c;
        }
    }
    
    return output;
}

// JSONオブジェクトからカラー文字列に変換する
std::string ColorToString(const std::string& color) {
    return color;
}

// 配列をJSONシリアライズする
template<typename T>
std::string SerializeToJsonArray(const std::vector<T>& data, 
                                std::function<std::string(const T&)> converter) {
    std::stringstream ss;
    ss << "[";
    bool first = true;
    
    for (const auto& item : data) {
        if (!first) {
            ss << ",";
        }
        first = false;
        ss << converter(item);
    }
    
    ss << "]";
    return ss.str();
}

// グラフタイプを文字列に変換する
std::string GraphTypeToString(ICGraphType type) {
    switch (type) {
        case ICGraphType::HitRateTimeSeries: return "hitRateTimeSeries";
        case ICGraphType::AccessTimeTimeSeries: return "accessTimeTimeSeries";
        case ICGraphType::TypeDistributionPie: return "typeDistributionPie";
        case ICGraphType::ResultDistributionPie: return "resultDistributionPie";
        case ICGraphType::HeatMap: return "heatMap";
        case ICGraphType::HistogramAccessTime: return "histogramAccessTime";
        case ICGraphType::NetworkGraph: return "networkGraph";
        case ICGraphType::ComparisonBar: return "comparisonBar";
        default: return "unknown";
    }
}

// カラーテーマを文字列に変換する
std::string ColorThemeToString(ICGraphColorTheme theme) {
    switch (theme) {
        case ICGraphColorTheme::Light: return "light";
        case ICGraphColorTheme::Dark: return "dark";
        case ICGraphColorTheme::Colorful: return "colorful";
        case ICGraphColorTheme::Monochrome: return "monochrome";
        case ICGraphColorTheme::Pastel: return "pastel";
        case ICGraphColorTheme::Contrast: return "contrast";
        default: return "light";
    }
}

// エクスポート形式を拡張子に変換する
std::string ExportFormatToExtension(ICVisualizationExportFormat format) {
    switch (format) {
        case ICVisualizationExportFormat::HTML: return ".html";
        case ICVisualizationExportFormat::SVG: return ".svg";
        case ICVisualizationExportFormat::PNG: return ".png";
        case ICVisualizationExportFormat::JSON: return ".json";
        case ICVisualizationExportFormat::CSV: return ".csv";
        default: return ".html";
    }
}

// テキストを文字列でエスケープする
std::string EscapeString(const std::string& text) {
    std::string result;
    result.reserve(text.size() + 10);
    
    for (char c : text) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '\"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    
    return result;
}

// データポイントをJSON文字列に変換する
std::string DataPointToJson(const ICGraphDataPoint& point) {
    std::stringstream ss;
    ss << "{\"x\":" << point.x
       << ",\"y\":" << point.y;
    
    if (!point.label.empty()) {
        ss << ",\"label\":\"" << EscapeString(point.label) << "\"";
    }
    
    ss << "}";
    return ss.str();
}

// データ系列をJSON文字列に変換する
std::string SeriesToJson(const ICGraphSeries& series) {
    std::stringstream ss;
    ss << "{\"name\":\"" << EscapeString(series.name) << "\","
       << "\"color\":\"" << EscapeString(series.color) << "\","
       << "\"visible\":" << (series.visible ? "true" : "false") << ","
       << "\"dataPoints\":" << SerializeToJsonArray<ICGraphDataPoint>(
              series.dataPoints, DataPointToJson)
       << "}";
    return ss.str();
}

// グラフ設定をJSON文字列に変換する
std::string GraphConfigToJson(const ICGraphConfig& config) {
    std::stringstream ss;
    ss << "{"
       << "\"title\":\"" << EscapeString(config.title) << "\","
       << "\"xAxisLabel\":\"" << EscapeString(config.xAxisLabel) << "\","
       << "\"yAxisLabel\":\"" << EscapeString(config.yAxisLabel) << "\","
       << "\"showLegend\":" << (config.showLegend ? "true" : "false") << ","
       << "\"showGrid\":" << (config.showGrid ? "true" : "false") << ","
       << "\"interactive\":" << (config.interactive ? "true" : "false") << ","
       << "\"showTooltips\":" << (config.showTooltips ? "true" : "false") << ","
       << "\"animation\":" << (config.animation ? "true" : "false") << ","
       << "\"colorTheme\":\"" << ColorThemeToString(config.colorTheme) << "\","
       << "\"width\":" << config.width << ","
       << "\"height\":" << config.height << ","
       << "\"customCssStyles\":\"" << EscapeString(config.customCssStyles) << "\""
       << "}";
    return ss.str();
}

// アクセス結果を文字列に変換する
std::string AccessResultToString(ICAccessResult result) {
    switch (result) {
        case ICAccessResult::Hit: return "Hit";
        case ICAccessResult::Miss: return "Miss";
        case ICAccessResult::Invalidated: return "Invalidated";
        case ICAccessResult::Overflow: return "Overflow";
        case ICAccessResult::TypeError: return "TypeError";
        case ICAccessResult::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

// キャッシュタイプを文字列に変換する
std::string CacheTypeToString(ICType type) {
    switch (type) {
        case ICType::Property: return "Property";
        case ICType::Method: return "Method";
        case ICType::Constructor: return "Constructor";
        case ICType::Prototype: return "Prototype";
        case ICType::Polymorphic: return "Polymorphic";
        case ICType::Megamorphic: return "Megamorphic";
        case ICType::Global: return "Global";
        case ICType::Builtin: return "Builtin";
        case ICType::Other: return "Other";
        default: return "Unknown";
    }
}

} // anonymous namespace

//==============================================================================
// ICVisualizer シングルトン実装
//==============================================================================

ICVisualizer& ICVisualizer::Instance() {
    static ICVisualizer instance;
    return instance;
}

ICVisualizer::ICVisualizer()
    : m_renderingEngine("d3")
    , m_defaultColorTheme(ICGraphColorTheme::Light)
    , m_defaultWidth(800)
    , m_defaultHeight(500)
    , m_customCssStyles("")
    , m_realtimeUpdatesEnabled(false)
    , m_realtimeUpdateInterval(1000) {
}

ICVisualizer::~ICVisualizer() {
    // リソースがあれば解放する
}

//==============================================================================
// ICVisualizer 公開メソッド
//==============================================================================

std::string ICVisualizer::GenerateHitRateTimeSeriesGraph(
    const std::vector<std::string>& cacheIds,
    const ICGraphConfig& config,
    uint64_t timeRange) {
    
    // 時系列データを準備
    auto series = PrepareTimeSeriesData(cacheIds, "hitRate", timeRange);
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Cache Hit Rate Over Time";
    }
    if (updatedConfig.yAxisLabel == "Value") {
        updatedConfig.yAxisLabel = "Hit Rate (%)";
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::HitRateTimeSeries,
        series,
        updatedConfig);
}

std::string ICVisualizer::GenerateAccessTimeSeriesGraph(
    const std::vector<std::string>& cacheIds,
    const ICGraphConfig& config,
    uint64_t timeRange) {
    
    // 時系列データを準備
    auto series = PrepareTimeSeriesData(cacheIds, "accessTime", timeRange);
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Cache Access Time Over Time";
    }
    if (updatedConfig.yAxisLabel == "Value") {
        updatedConfig.yAxisLabel = "Access Time (ns)";
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::AccessTimeTimeSeries,
        series,
        updatedConfig);
}

std::string ICVisualizer::GenerateTypeDistributionPieChart(const ICGraphConfig& config) {
    // キャッシュタイプの分布データを収集
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    
    std::map<ICType, int> typeCounts;
    std::vector<std::string> cacheIds = analyzer.GetAllCacheIds();
    
    for (const auto& cacheId : cacheIds) {
        auto cacheInfo = analyzer.GetCacheInfo(cacheId);
        if (cacheInfo) {
            typeCounts[cacheInfo->type]++;
        }
    }
    
    // ラベルと値のベクトルを作成
    std::vector<std::string> labels;
    std::vector<double> values;
    
    for (const auto& pair : typeCounts) {
        labels.push_back(CacheTypeToString(pair.first));
        values.push_back(static_cast<double>(pair.second));
    }
    
    // 円グラフ用データを準備
    auto series = PreparePieChartData(labels, values, "Cache Types");
    std::vector<ICGraphSeries> seriesVector = {series};
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Cache Type Distribution";
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::TypeDistributionPie,
        seriesVector,
        updatedConfig);
}

std::string ICVisualizer::GenerateResultDistributionPieChart(
    const std::string& cacheId,
    const ICGraphConfig& config) {
    
    // アクセス結果の分布データを収集
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    
    std::map<ICAccessResult, uint64_t> resultCounts;
    std::vector<std::string> cacheIds;
    
    if (cacheId.empty()) {
        cacheIds = analyzer.GetAllCacheIds();
    } else {
        cacheIds.push_back(cacheId);
    }
    
    for (const auto& id : cacheIds) {
        auto stats = analyzer.GetCacheStats(id);
        if (stats) {
            resultCounts[ICAccessResult::Hit] += stats->hits.load();
            resultCounts[ICAccessResult::Miss] += stats->misses.load();
            resultCounts[ICAccessResult::Invalidated] += stats->invalidations.load();
            resultCounts[ICAccessResult::Overflow] += stats->overflows.load();
            // その他の結果タイプも必要に応じて集計
        }
    }
    
    // ラベルと値のベクトルを作成
    std::vector<std::string> labels;
    std::vector<double> values;
    
    for (const auto& pair : resultCounts) {
        if (pair.second > 0) {  // 0より大きい値だけを含める
            labels.push_back(AccessResultToString(pair.first));
            values.push_back(static_cast<double>(pair.second));
        }
    }
    
    // 円グラフ用データを準備
    auto series = PreparePieChartData(labels, values, "Access Results");
    std::vector<ICGraphSeries> seriesVector = {series};
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Cache Access Result Distribution";
        if (!cacheId.empty()) {
            updatedConfig.title += " for " + cacheId;
        }
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::ResultDistributionPie,
        seriesVector,
        updatedConfig);
}

std::string ICVisualizer::GenerateHeatMapGraph(
    const std::vector<std::string>& cacheIds,
    const ICGraphConfig& config) {
    
    // ヒートマップデータを準備
    std::map<std::string, std::map<std::string, double>> heatmapData;
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    
    // キャッシュIDとその場所（関数、ファイルなど）をマッピング
    std::vector<std::string> xLabels; // 関数名
    std::vector<std::string> yLabels; // キャッシュタイプ
    std::map<std::string, std::map<std::string, double>> valueMatrix;
    
    // 対象となるキャッシュIDが指定されていない場合は全キャッシュを取得
    std::vector<std::string> targetCacheIds = cacheIds;
    if (targetCacheIds.empty()) {
        targetCacheIds = analyzer.GetAllCacheIds();
    }
    
    // 関数・場所とキャッシュタイプごとのヒット率を集計
    for (const auto& cacheId : targetCacheIds) {
        auto info = analyzer.GetCacheInfo(cacheId);
        auto stats = analyzer.GetCacheStats(cacheId);
        
        if (info && stats) {
            std::string functionName = info->location;
            std::string cacheType = CacheTypeToString(info->type);
            
            // ラベルのセットを構築
            if (std::find(xLabels.begin(), xLabels.end(), functionName) == xLabels.end()) {
                xLabels.push_back(functionName);
            }
            if (std::find(yLabels.begin(), yLabels.end(), cacheType) == yLabels.end()) {
                yLabels.push_back(cacheType);
            }
            
            // ヒット率を計算
            uint64_t hits = stats->hits.load();
            uint64_t total = hits + stats->misses.load();
            double hitRate = (total > 0) ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
            
            // 値を追加または更新
            auto& typeStats = valueMatrix[functionName];
            if (typeStats.find(cacheType) != typeStats.end()) {
                // すでに存在する場合は平均を取る
                typeStats[cacheType] = (typeStats[cacheType] + hitRate) / 2.0;
            } else {
                typeStats[cacheType] = hitRate;
            }
        }
    }
    
    // ヒートマップ用データを準備
    auto heatmapSeries = PrepareHeatmapData(xLabels, yLabels, valueMatrix);
    std::vector<ICGraphSeries> seriesVector = {heatmapSeries};
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Cache Hit Rate Heatmap by Function and Type";
    }
    if (updatedConfig.xAxisLabel == "X") {
        updatedConfig.xAxisLabel = "Function / Location";
    }
    if (updatedConfig.yAxisLabel == "Y") {
        updatedConfig.yAxisLabel = "Cache Type";
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::HeatMap,
        seriesVector,
        updatedConfig);
}

std::string ICVisualizer::GenerateHistogramGraph(
    const std::string& cacheId,
    const ICGraphConfig& config,
    uint64_t binCount) {
    
    // アクセス時間のヒストグラムデータを準備
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    std::vector<ICGraphSeries> seriesVector;
    
    if (cacheId.empty()) {
        // 全キャッシュの平均アクセス時間を取得
        auto allCacheIds = analyzer.GetAllCacheIds();
        std::vector<double> allAccessTimes;
        
        for (const auto& id : allCacheIds) {
            auto measurements = analyzer.GetAccessTimeMeasurements(id);
            allAccessTimes.insert(allAccessTimes.end(), measurements.begin(), measurements.end());
        }
        
        auto histogramSeries = PrepareHistogramData(allAccessTimes, binCount, "All Caches");
        seriesVector.push_back(histogramSeries);
    } else {
        // 指定されたキャッシュのアクセス時間を取得
        auto measurements = analyzer.GetAccessTimeMeasurements(cacheId);
        auto histogramSeries = PrepareHistogramData(measurements, binCount, cacheId);
        seriesVector.push_back(histogramSeries);
    }
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Access Time Distribution";
        if (!cacheId.empty()) {
            updatedConfig.title += " for " + cacheId;
        }
    }
    if (updatedConfig.xAxisLabel == "X") {
        updatedConfig.xAxisLabel = "Access Time (ns)";
    }
    if (updatedConfig.yAxisLabel == "Y") {
        updatedConfig.yAxisLabel = "Frequency";
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::HistogramAccessTime,
        seriesVector,
        updatedConfig);
}

std::string ICVisualizer::GenerateNetworkGraph(
    const std::vector<std::string>& cacheIds,
    const ICGraphConfig& config) {
    
    // ネットワークグラフのノードとエッジを準備
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    std::vector<ICGraphNode> nodes;
    std::vector<ICGraphEdge> edges;
    
    // 対象となるキャッシュIDが指定されていない場合は全キャッシュを取得
    std::vector<std::string> targetCacheIds = cacheIds;
    if (targetCacheIds.empty()) {
        targetCacheIds = analyzer.GetAllCacheIds();
    }
    
    std::map<std::string, size_t> nodeIndices;
    
    // ノードを構築
    for (const auto& cacheId : targetCacheIds) {
        auto info = analyzer.GetCacheInfo(cacheId);
        auto stats = analyzer.GetCacheStats(cacheId);
        
        if (info && stats) {
            ICGraphNode node;
            node.id = cacheId;
            node.label = info->name;
            node.group = static_cast<int>(info->type);
            
            // ヒット率を計算しノードのサイズに使用
            uint64_t hits = stats->hits.load();
            uint64_t total = hits + stats->misses.load();
            double hitRate = (total > 0) ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
            node.size = 10.0 + (hitRate / 10.0); // 基本サイズ + ヒット率による調整
            
            // アクセス数をノードの色に使用（多いほど濃い色）
            node.color = "#" + std::to_string(std::min(255, static_cast<int>(total / 100))) + 
                         "3080"; // 赤緑青の値を調整
            
            nodeIndices[cacheId] = nodes.size();
            nodes.push_back(node);
        }
    }
    
    // エッジを構築（依存関係または相互関係に基づく）
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& sourceId = nodes[i].id;
        auto sourceDependencies = analyzer.GetCacheDependencies(sourceId);
        
        for (const auto& targetId : sourceDependencies) {
            auto it = nodeIndices.find(targetId);
            if (it != nodeIndices.end()) {
                ICGraphEdge edge;
                edge.source = i;
                edge.target = it->second;
                edge.value = 1.0; // デフォルト値、必要に応じて調整
                
                edges.push_back(edge);
            }
        }
    }
    
    // ネットワークグラフ用データを準備
    auto networkSeries = PrepareNetworkData(nodes, edges, "Cache Dependencies");
    std::vector<ICGraphSeries> seriesVector = {networkSeries};
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        updatedConfig.title = "Cache Dependency Network";
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::NetworkGraph,
        seriesVector,
        updatedConfig);
}

std::string ICVisualizer::GenerateComparisonBarGraph(
    const std::vector<std::string>& cacheIds,
    const ICGraphConfig& config,
    const std::string& metricType) {
    
    // 比較用の棒グラフデータを準備
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    std::vector<std::string> labels;
    std::vector<double> values;
    
    // 対象となるキャッシュIDが指定されていない場合は全キャッシュを取得
    std::vector<std::string> targetCacheIds = cacheIds;
    if (targetCacheIds.empty()) {
        targetCacheIds = analyzer.GetAllCacheIds();
        
        // 多すぎる場合は上位10個に制限
        if (targetCacheIds.size() > 10) {
            std::vector<std::pair<std::string, double>> sortedCaches;
            
            for (const auto& id : targetCacheIds) {
                auto stats = analyzer.GetCacheStats(id);
                if (stats) {
                    double value = 0.0;
                    
                    if (metricType == "hitRate") {
                        uint64_t hits = stats->hits.load();
                        uint64_t total = hits + stats->misses.load();
                        value = (total > 0) ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
                    } else if (metricType == "accessCount") {
                        value = static_cast<double>(stats->hits.load() + stats->misses.load());
                    } else if (metricType == "accessTime") {
                        auto measurements = analyzer.GetAccessTimeMeasurements(id);
                        if (!measurements.empty()) {
                            value = std::accumulate(measurements.begin(), measurements.end(), 0.0) / 
                                    measurements.size();
                        }
                    }
                    
                    sortedCaches.emplace_back(id, value);
                }
            }
            
            // 値でソート（降順）
            std::sort(sortedCaches.begin(), sortedCaches.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
            
            // 上位10個を選択
            targetCacheIds.clear();
            for (size_t i = 0; i < std::min(size_t(10), sortedCaches.size()); ++i) {
                targetCacheIds.push_back(sortedCaches[i].first);
            }
        }
    }
    
    // 指定されたメトリックに基づいて値を収集
    for (const auto& id : targetCacheIds) {
        auto info = analyzer.GetCacheInfo(id);
        auto stats = analyzer.GetCacheStats(id);
        
        if (info && stats) {
            std::string label = info->name;
            double value = 0.0;
            
            if (metricType == "hitRate") {
                uint64_t hits = stats->hits.load();
                uint64_t total = hits + stats->misses.load();
                value = (total > 0) ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
            } else if (metricType == "accessCount") {
                value = static_cast<double>(stats->hits.load() + stats->misses.load());
            } else if (metricType == "accessTime") {
                auto measurements = analyzer.GetAccessTimeMeasurements(id);
                if (!measurements.empty()) {
                    value = std::accumulate(measurements.begin(), measurements.end(), 0.0) / 
                            measurements.size();
                }
            } else if (metricType == "invalidations") {
                value = static_cast<double>(stats->invalidations.load());
            } else if (metricType == "overflows") {
                value = static_cast<double>(stats->overflows.load());
            }
            
            labels.push_back(label);
            values.push_back(value);
        }
    }
    
    // 棒グラフ用データを準備
    auto barSeries = PrepareBarChartData(labels, values, "Caches");
    std::vector<ICGraphSeries> seriesVector = {barSeries};
    
    // クローン設定を作成して必要に応じて上書き
    ICGraphConfig updatedConfig = config;
    if (updatedConfig.title == "Inline Cache Performance") {
        if (metricType == "hitRate") {
            updatedConfig.title = "Cache Hit Rate Comparison";
        } else if (metricType == "accessCount") {
            updatedConfig.title = "Cache Access Count Comparison";
        } else if (metricType == "accessTime") {
            updatedConfig.title = "Cache Average Access Time Comparison";
        } else if (metricType == "invalidations") {
            updatedConfig.title = "Cache Invalidation Count Comparison";
        } else if (metricType == "overflows") {
            updatedConfig.title = "Cache Overflow Count Comparison";
        }
    }
    
    if (updatedConfig.yAxisLabel == "Value") {
        if (metricType == "hitRate") {
            updatedConfig.yAxisLabel = "Hit Rate (%)";
        } else if (metricType == "accessCount") {
            updatedConfig.yAxisLabel = "Access Count";
        } else if (metricType == "accessTime") {
            updatedConfig.yAxisLabel = "Average Access Time (ns)";
        } else if (metricType == "invalidations") {
            updatedConfig.yAxisLabel = "Invalidation Count";
        } else if (metricType == "overflows") {
            updatedConfig.yAxisLabel = "Overflow Count";
        }
    }
    
    // エンジンを使用してグラフをレンダリング
    return RenderGraphWithEngine(
        ICGraphType::ComparisonBar,
        seriesVector,
        updatedConfig);
}

std::string ICVisualizer::GenerateDashboard(
    const std::vector<std::string>& cacheIds,
    const ICGraphConfig& config) {
    
    std::vector<std::string> graphs;
    ICGraphConfig dashboardConfig = config;
    
    // ダッシュボード向けに設定を調整
    dashboardConfig.width = config.width / 2 - 20;
    dashboardConfig.height = config.height / 2 - 20;
    
    // ヒット率時系列グラフを追加
    graphs.push_back(GenerateHitRateTimeSeriesGraph(cacheIds, dashboardConfig));
    
    // アクセス時間時系列グラフを追加
    graphs.push_back(GenerateAccessTimeSeriesGraph(cacheIds, dashboardConfig));
    
    // キャッシュタイプ分布円グラフを追加
    graphs.push_back(GenerateTypeDistributionPieChart(dashboardConfig));
    
    // アクセス結果分布円グラフを追加
    graphs.push_back(GenerateResultDistributionPieChart("", dashboardConfig));
    
    // ダッシュボードHTMLを生成
    return GenerateDashboardHTML(graphs, config);
}

std::string ICVisualizer::ExportVisualization(
    const std::string& graphHtml,
    const std::string& filename,
    ICVisualizationExportFormat format) {
    
    // エクスポート用のファイル名を生成
    std::string baseFilename = filename;
    if (baseFilename.empty()) {
        baseFilename = "ic_visualization_" + 
                       std::to_string(
                           std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    std::string extension = ExportFormatToExtension(format);
    std::string fullFilename = baseFilename + extension;
    
    // 形式に応じてエクスポート処理
    switch (format) {
        case ICVisualizationExportFormat::HTML:
            ExportToHTML(graphHtml, fullFilename);
            break;
            
        case ICVisualizationExportFormat::SVG:
            ExportToSVG(graphHtml, fullFilename);
            break;
            
        case ICVisualizationExportFormat::PNG:
            ExportToPNG(graphHtml, fullFilename);
            break;
            
        case ICVisualizationExportFormat::JSON:
            ExportToJSON(graphHtml, fullFilename);
            break;
            
        case ICVisualizationExportFormat::CSV:
            ExportToCSV(graphHtml, fullFilename);
            break;
            
        default:
            ExportToHTML(graphHtml, fullFilename);
            break;
    }
    
    return fullFilename;
}

void ICVisualizer::SetRenderingEngine(const std::string& engineName) {
    m_renderingEngine = engineName;
}

std::string ICVisualizer::GetRenderingEngine() const {
    return m_renderingEngine;
}

void ICVisualizer::SetDefaultColorTheme(ICGraphColorTheme theme) {
    m_defaultColorTheme = theme;
}

ICGraphColorTheme ICVisualizer::GetDefaultColorTheme() const {
    return m_defaultColorTheme;
}

void ICVisualizer::SetDefaultDimensions(uint32_t width, uint32_t height) {
    m_defaultWidth = width;
    m_defaultHeight = height;
}

std::pair<uint32_t, uint32_t> ICVisualizer::GetDefaultDimensions() const {
    return std::make_pair(m_defaultWidth, m_defaultHeight);
}

void ICVisualizer::SetCustomCssStyles(const std::string& styles) {
    m_customCssStyles = styles;
}

std::string ICVisualizer::GetCustomCssStyles() const {
    return m_customCssStyles;
}

void ICVisualizer::EnableRealtimeUpdates(bool enable, uint32_t updateIntervalMs) {
    m_realtimeUpdatesEnabled = enable;
    if (updateIntervalMs > 0) {
        m_realtimeUpdateInterval = updateIntervalMs;
    }
}

bool ICVisualizer::AreRealtimeUpdatesEnabled() const {
    return m_realtimeUpdatesEnabled;
}

uint32_t ICVisualizer::GetRealtimeUpdateInterval() const {
    return m_realtimeUpdateInterval;
}

ICGraphConfig ICVisualizer::CreateDefaultGraphConfig() const {
    ICGraphConfig config;
    config.title = "Inline Cache Performance";
    config.xAxisLabel = "Time";
    config.yAxisLabel = "Value";
    config.showLegend = true;
    config.showGrid = true;
    config.interactive = true;
    config.showTooltips = true;
    config.animation = true;
    config.colorTheme = m_defaultColorTheme;
    config.width = m_defaultWidth;
    config.height = m_defaultHeight;
    config.customCssStyles = m_customCssStyles;
    return config;
}

//==============================================================================
// ICVisualizer 内部ヘルパーメソッド
//==============================================================================

std::vector<ICGraphSeries> ICVisualizer::PrepareTimeSeriesData(
    const std::vector<std::string>& cacheIds,
    const std::string& metricType,
    uint64_t timeRange) {
    
    std::vector<ICGraphSeries> result;
    const auto& analyzer = ICPerformanceAnalyzer::Instance();
    
    // 対象となるキャッシュIDが指定されていない場合は全キャッシュから上位5個を選択
    std::vector<std::string> targetCacheIds = cacheIds;
    if (targetCacheIds.empty()) {
        auto allCacheIds = analyzer.GetAllCacheIds();
        
        // アクセス数でソートして上位5個を選択
        std::vector<std::pair<std::string, uint64_t>> sortedCaches;
        for (const auto& id : allCacheIds) {
            auto stats = analyzer.GetCacheStats(id);
            if (stats) {
                uint64_t accessCount = stats->hits.load() + stats->misses.load();
                sortedCaches.emplace_back(id, accessCount);
            }
        }
        
        std::sort(sortedCaches.begin(), sortedCaches.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        size_t count = std::min(size_t(5), sortedCaches.size());
        targetCacheIds.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            targetCacheIds.push_back(sortedCaches[i].first);
        }
    }
    
    // 各キャッシュの時系列データを取得
    for (const auto& cacheId : targetCacheIds) {
        auto info = analyzer.GetCacheInfo(cacheId);
        if (!info) continue;
        
        ICGraphSeries series;
        series.name = info->name;
        series.visible = true;
        
        // カラーコードの生成（キャッシュIDに基づく一貫した色）
        std::hash<std::string> hasher;
        size_t hash = hasher(cacheId);
        std::stringstream colorStream;
        colorStream << "#" << std::hex << std::setw(6) << std::setfill('0') 
                  << (hash % 0xFFFFFF);
        series.color = colorStream.str();
        
        // 時系列データポイントを取得
        std::vector<ICTimestampedValue> timeseriesData;
        
        if (metricType == "hitRate") {
            timeseriesData = analyzer.GetHitRateTimeSeries(cacheId, timeRange);
        } else if (metricType == "accessTime") {
            timeseriesData = analyzer.GetAccessTimeTimeSeries(cacheId, timeRange);
        }
        
        // データポイントに変換
        for (const auto& point : timeseriesData) {
            ICGraphDataPoint dataPoint;
            dataPoint.x = static_cast<double>(point.timestamp);
            dataPoint.y = point.value;
            series.dataPoints.push_back(dataPoint);
        }
        
        if (!series.dataPoints.empty()) {
            result.push_back(series);
        }
    }
    
    return result;
}
} // namespace core
} // namespace aerojs 