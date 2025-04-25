#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>

#include "ic_logger.h"
#include "ic_performance_analyzer.h"

namespace aerojs {
namespace core {

// 前方宣言
class InlineCacheManager;

/**
 * @brief 可視化の出力形式
 */
enum class ICVisualizationFormat {
    DOT,       // Graphvizのドット形式
    JSON,      // JSON形式
    HTML,      // HTML形式
    SVG,       // SVG形式
    PNG,       // PNG形式（外部ツールが必要）
    TXT        // プレーンテキスト形式
};

/**
 * @brief 可視化の詳細レベル
 */
enum class ICVisualizationDetailLevel {
    Minimal,   // 最小限の情報
    Basic,     // 基本情報
    Detailed,  // 詳細情報
    Complete   // 完全な情報
};

/**
 * @brief 可視化のスタイル設定
 */
struct ICVisualizationStyle {
    std::string fontName;               // フォント名
    int fontSize;                       // フォントサイズ
    std::string backgroundColor;        // 背景色
    std::string nodeColor;              // ノードの色
    std::string edgeColor;              // エッジの色
    std::string highlightColor;         // ハイライト色
    std::string warningColor;           // 警告色
    std::string errorColor;             // エラー色
    bool colorByHitRate;                // ヒット率で色付けするかどうか
    bool colorByType;                   // タイプで色付けするかどうか
    bool useGradients;                  // グラデーションを使用するかどうか
    bool showLabels;                    // ラベルを表示するかどうか
    bool showLegend;                    // 凡例を表示するかどうか
    bool showStatistics;                // 統計情報を表示するかどうか
    int nodeSize;                       // ノードのサイズ
    int edgeThickness;                  // エッジの太さ
    
    // デフォルトコンストラクタ
    ICVisualizationStyle() 
        : fontName("Arial")
        , fontSize(10)
        , backgroundColor("#ffffff")
        , nodeColor("#4286f4")
        , edgeColor("#888888")
        , highlightColor("#ff9900")
        , warningColor("#ff4500")
        , errorColor("#ff0000")
        , colorByHitRate(true)
        , colorByType(true)
        , useGradients(true)
        , showLabels(true)
        , showLegend(true)
        , showStatistics(true)
        , nodeSize(50)
        , edgeThickness(1)
    {
    }
};

/**
 * @brief 可視化オプション
 */
struct ICVisualizationOptions {
    ICVisualizationFormat format;         // 出力形式
    ICVisualizationDetailLevel detailLevel; // 詳細レベル
    ICVisualizationStyle style;           // スタイル設定
    std::string outputPath;               // 出力パス
    bool includePerformanceData;          // パフォーマンスデータを含めるかどうか
    bool includeOptimizationHistory;      // 最適化履歴を含めるかどうか
    bool highlightProblematicCaches;      // 問題のあるキャッシュをハイライトするかどうか
    bool includeRelatedCaches;            // 関連するキャッシュを含めるかどうか
    bool showShapeInformation;            // オブジェクト形状情報を表示するかどうか
    bool limitToTopCaches;                // 上位のキャッシュのみに制限するかどうか
    size_t topCachesLimit;                // 上位のキャッシュの数
    std::vector<std::string> targetCacheIds; // 対象とするキャッシュIDのリスト
    std::vector<ICType> targetCacheTypes;  // 対象とするキャッシュタイプのリスト
    
    // デフォルトコンストラクタ
    ICVisualizationOptions()
        : format(ICVisualizationFormat::DOT)
        , detailLevel(ICVisualizationDetailLevel::Basic)
        , outputPath("")
        , includePerformanceData(true)
        , includeOptimizationHistory(false)
        , highlightProblematicCaches(true)
        , includeRelatedCaches(true)
        , showShapeInformation(false)
        , limitToTopCaches(false)
        , topCachesLimit(10)
    {
    }
};

/**
 * @brief ノードデータ構造
 */
struct ICVisualizationNode {
    std::string id;                     // ノードID
    std::string label;                  // ノードラベル
    std::string color;                  // ノード色
    std::string shape;                  // ノード形状
    std::string tooltip;                // ツールチップ
    std::string url;                    // リンクURL
    std::unordered_map<std::string, std::string> attributes; // その他の属性
    
    ICVisualizationNode(const std::string& nodeId = "")
        : id(nodeId)
        , shape("box")
    {
    }
};

/**
 * @brief エッジデータ構造
 */
struct ICVisualizationEdge {
    std::string sourceId;               // ソースノードID
    std::string targetId;               // ターゲットノードID
    std::string label;                  // エッジラベル
    std::string color;                  // エッジ色
    std::string style;                  // エッジスタイル
    int weight;                         // エッジの重み
    std::string tooltip;                // ツールチップ
    std::unordered_map<std::string, std::string> attributes; // その他の属性
    
    ICVisualizationEdge(
        const std::string& source = "", 
        const std::string& target = "")
        : sourceId(source)
        , targetId(target)
        , style("solid")
        , weight(1)
    {
    }
};

/**
 * @brief グラフデータ構造
 */
struct ICVisualizationGraph {
    std::string title;                  // グラフのタイトル
    std::unordered_map<std::string, ICVisualizationNode> nodes; // ノードマップ
    std::vector<ICVisualizationEdge> edges; // エッジリスト
    std::unordered_map<std::string, std::string> attributes; // グラフ属性
    
    ICVisualizationGraph(const std::string& graphTitle = "")
        : title(graphTitle)
    {
    }
    
    // ノードの追加
    void AddNode(const ICVisualizationNode& node) {
        nodes[node.id] = node;
    }
    
    // エッジの追加
    void AddEdge(const ICVisualizationEdge& edge) {
        edges.push_back(edge);
    }
};

/**
 * @brief インラインキャッシュ可視化器クラス
 */
class ICVisualizer {
public:
    /**
     * @brief シングルトンインスタンスの取得
     * @return ICVisualizerのシングルトンインスタンス
     */
    static ICVisualizer& Instance();
    
    /**
     * @brief デストラクタ
     */
    ~ICVisualizer();
    
    /**
     * @brief 指定したキャッシュIDのキャッシュを可視化する
     * @param cacheId キャッシュID
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string VisualizeCache(
        const std::string& cacheId,
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief 指定したタイプのすべてのキャッシュを可視化する
     * @param type キャッシュタイプ
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string VisualizeCachesByType(
        ICType type,
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief すべてのキャッシュを可視化する
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string VisualizeAllCaches(
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief キャッシュの関係性を可視化する
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string VisualizeCacheRelationships(
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief キャッシュのパフォーマンス指標を可視化する
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string VisualizePerformanceMetrics(
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief キャッシュの最適化履歴を可視化する
     * @param cacheId キャッシュID
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string VisualizeOptimizationHistory(
        const std::string& cacheId,
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief キャッシュのヒートマップを生成する
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化結果の文字列
     */
    std::string GenerateCacheHeatmap(
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief 指定形式でグラフを出力する
     * @param graph 可視化グラフ
     * @param format 出力形式
     * @param outputPath 出力パス
     * @return 可視化結果の文字列
     */
    std::string ExportGraph(
        const ICVisualizationGraph& graph,
        ICVisualizationFormat format,
        const std::string& outputPath = "");
    
    /**
     * @brief 指定したファイルにグラフを保存する
     * @param graph 可視化グラフ
     * @param outputPath 出力パス
     * @param format 出力形式
     * @return 保存が成功したかどうか
     */
    bool SaveGraphToFile(
        const ICVisualizationGraph& graph,
        const std::string& outputPath,
        ICVisualizationFormat format);
    
    /**
     * @brief 外部コマンドで可視化結果を表示する
     * @param graph 可視化グラフ
     * @param format 出力形式
     * @return 表示が成功したかどうか
     */
    bool DisplayGraph(
        const ICVisualizationGraph& graph,
        ICVisualizationFormat format);
    
    /**
     * @brief カスタム可視化ハンドラを登録する
     * @param format 出力形式
     * @param handler 可視化ハンドラ関数
     */
    void RegisterCustomVisualizer(
        ICVisualizationFormat format,
        std::function<std::string(const ICVisualizationGraph&)> handler);

private:
    /**
     * @brief コンストラクタ（シングルトンパターン）
     */
    ICVisualizer();
    
    // コピー禁止
    ICVisualizer(const ICVisualizer&) = delete;
    ICVisualizer& operator=(const ICVisualizer&) = delete;
    
    /**
     * @brief DOT形式でグラフを出力する
     * @param graph 可視化グラフ
     * @return DOT形式の文字列
     */
    std::string GenerateDotOutput(const ICVisualizationGraph& graph);
    
    /**
     * @brief JSON形式でグラフを出力する
     * @param graph 可視化グラフ
     * @return JSON形式の文字列
     */
    std::string GenerateJsonOutput(const ICVisualizationGraph& graph);
    
    /**
     * @brief HTML形式でグラフを出力する
     * @param graph 可視化グラフ
     * @return HTML形式の文字列
     */
    std::string GenerateHtmlOutput(const ICVisualizationGraph& graph);
    
    /**
     * @brief SVG形式でグラフを出力する
     * @param graph 可視化グラフ
     * @return SVG形式の文字列
     */
    std::string GenerateSvgOutput(const ICVisualizationGraph& graph);
    
    /**
     * @brief プレーンテキスト形式でグラフを出力する
     * @param graph 可視化グラフ
     * @return テキスト形式の文字列
     */
    std::string GenerateTextOutput(const ICVisualizationGraph& graph);
    
    /**
     * @brief インラインキャッシュからノードを構築する
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 可視化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 可視化ノード
     */
    ICVisualizationNode BuildCacheNode(
        const std::string& cacheId,
        ICType type,
        const ICVisualizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief 2つのキャッシュ間のエッジを構築する
     * @param sourceCacheId ソースキャッシュID
     * @param targetCacheId ターゲットキャッシュID
     * @param relationshipType 関係タイプ
     * @param options 可視化オプション
     * @return 可視化エッジ
     */
    ICVisualizationEdge BuildCacheEdge(
        const std::string& sourceCacheId,
        const std::string& targetCacheId,
        const std::string& relationshipType,
        const ICVisualizationOptions& options);
    
    /**
     * @brief ヒット率に基づいて色を取得する
     * @param hitRate ヒット率
     * @return 色コード
     */
    std::string GetColorForHitRate(double hitRate);
    
    /**
     * @brief キャッシュタイプに基づいて色を取得する
     * @param type キャッシュタイプ
     * @return 色コード
     */
    std::string GetColorForCacheType(ICType type);
    
    /**
     * @brief キャッシュの関連性を取得する
     * @param cacheId キャッシュID
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 関連するキャッシュIDのマップ
     */
    std::unordered_map<std::string, std::string> GetRelatedCaches(
        const std::string& cacheId,
        InlineCacheManager* cacheManager);
    
    // カスタム可視化ハンドラのマップ
    std::unordered_map<ICVisualizationFormat, 
        std::function<std::string(const ICVisualizationGraph&)>> m_customVisualizers;
    
    // ミューテックス
    mutable std::mutex m_visualizersMutex;
};

} // namespace core
} // namespace aerojs 