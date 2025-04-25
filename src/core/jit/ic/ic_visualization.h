#pragma once

#include "ic_performance_analyzer.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <atomic>

namespace aerojs {
namespace core {

/**
 * @brief インラインキャッシュ視覚化のグラフタイプ
 */
enum class ICGraphType {
    HitRateTimeSeries,        // ヒット率の時系列グラフ
    AccessTimeTimeSeries,     // アクセス時間の時系列グラフ
    TypeDistributionPie,      // キャッシュタイプ分布の円グラフ
    ResultDistributionPie,    // アクセス結果分布の円グラフ
    HeatMap,                  // ホットスポットのヒートマップ
    HistogramAccessTime,      // アクセス時間のヒストグラム
    NetworkGraph,             // キャッシュ間の関係ネットワークグラフ
    ComparisonBar             // 複数キャッシュの比較バーグラフ
};

/**
 * @brief グラフのカラーテーマ
 */
enum class ICGraphColorTheme {
    Light,      // 明るいテーマ
    Dark,       // 暗いテーマ
    Colorful,   // カラフルテーマ
    Monochrome, // モノクロテーマ
    Pastel,     // パステルテーマ
    Contrast    // 高コントラストテーマ
};

/**
 * @brief グラフデータポイント
 */
struct ICGraphDataPoint {
    double x;
    double y;
    std::string label;
    
    ICGraphDataPoint(double xVal = 0.0, double yVal = 0.0, const std::string& lbl = "")
        : x(xVal), y(yVal), label(lbl) {}
};

/**
 * @brief データ系列
 */
struct ICGraphSeries {
    std::string name;
    std::vector<ICGraphDataPoint> dataPoints;
    std::string color;
    bool visible;
    
    ICGraphSeries(const std::string& seriesName = "", const std::string& seriesColor = "#1f77b4")
        : name(seriesName), color(seriesColor), visible(true) {}
};

/**
 * @brief グラフ設定
 */
struct ICGraphConfig {
    std::string title;
    std::string xAxisLabel;
    std::string yAxisLabel;
    bool showLegend;
    bool showGrid;
    bool interactive;
    bool showTooltips;
    bool animation;
    ICGraphColorTheme colorTheme;
    int width;
    int height;
    std::string customCssStyles;
    
    ICGraphConfig()
        : title("Inline Cache Performance")
        , xAxisLabel("Time")
        , yAxisLabel("Value")
        , showLegend(true)
        , showGrid(true)
        , interactive(true)
        , showTooltips(true)
        , animation(true)
        , colorTheme(ICGraphColorTheme::Light)
        , width(800)
        , height(500)
        , customCssStyles("") {}
};

/**
 * @brief 保存されたグラフの設定
 */
struct ICSavedGraph {
    std::string id;
    ICGraphType type;
    ICGraphConfig config;
    std::vector<ICGraphSeries> series;
    std::chrono::system_clock::time_point creationTime;
    std::string cacheId;  // 関連するキャッシュID（該当する場合）
};

/**
 * @brief ビジュアライゼーションエクスポート形式
 */
enum class ICVisualizationExportFormat {
    HTML,    // HTML形式（インタラクティブ）
    SVG,     // SVG形式
    PNG,     // PNG画像形式
    JSON,    // JSONデータ形式
    CSV      // CSVデータ形式
};

/**
 * @brief インラインキャッシュビジュアライザークラス
 * キャッシュパフォーマンスデータを視覚化するためのユーティリティ
 */
class ICVisualizer {
public:
    /**
     * @brief シングルトンインスタンスを取得
     * @return ICVisualizerのシングルトンインスタンス
     */
    static ICVisualizer& Instance();

    /**
     * @brief デストラクタ
     */
    ~ICVisualizer();

    /**
     * @brief ヒット率の時系列グラフを生成
     * @param cacheIds キャッシュIDのリスト（空の場合は全キャッシュの集計）
     * @param config グラフ設定
     * @param timeRange 時間範囲（ミリ秒）、0の場合は全履歴
     * @return グラフを表すHTML文字列
     */
    std::string GenerateHitRateTimeSeriesGraph(
        const std::vector<std::string>& cacheIds,
        const ICGraphConfig& config = ICGraphConfig(),
        uint64_t timeRange = 0);

    /**
     * @brief アクセス時間の時系列グラフを生成
     * @param cacheIds キャッシュIDのリスト（空の場合は全キャッシュの集計）
     * @param config グラフ設定
     * @param timeRange 時間範囲（ミリ秒）、0の場合は全履歴
     * @return グラフを表すHTML文字列
     */
    std::string GenerateAccessTimeSeriesGraph(
        const std::vector<std::string>& cacheIds,
        const ICGraphConfig& config = ICGraphConfig(),
        uint64_t timeRange = 0);

    /**
     * @brief キャッシュタイプ分布の円グラフを生成
     * @param config グラフ設定
     * @return グラフを表すHTML文字列
     */
    std::string GenerateTypeDistributionPieChart(const ICGraphConfig& config = ICGraphConfig());

    /**
     * @brief アクセス結果分布の円グラフを生成
     * @param cacheId キャッシュID（空の場合は全キャッシュの集計）
     * @param config グラフ設定
     * @return グラフを表すHTML文字列
     */
    std::string GenerateResultDistributionPieChart(
        const std::string& cacheId = "",
        const ICGraphConfig& config = ICGraphConfig());

    /**
     * @brief ホットスポットのヒートマップを生成
     * @param metric 測定指標（"hitRate", "accessTime", "invalidations"など）
     * @param config グラフ設定
     * @return グラフを表すHTML文字列
     */
    std::string GenerateHeatMap(
        const std::string& metric,
        const ICGraphConfig& config = ICGraphConfig());

    /**
     * @brief アクセス時間のヒストグラムを生成
     * @param cacheId キャッシュID（空の場合は全キャッシュの集計）
     * @param bins ビン（バケット）の数
     * @param config グラフ設定
     * @return グラフを表すHTML文字列
     */
    std::string GenerateAccessTimeHistogram(
        const std::string& cacheId = "",
        int bins = 10,
        const ICGraphConfig& config = ICGraphConfig());

    /**
     * @brief キャッシュ間の関係ネットワークグラフを生成
     * @param config グラフ設定
     * @return グラフを表すHTML文字列
     */
    std::string GenerateNetworkGraph(const ICGraphConfig& config = ICGraphConfig());

    /**
     * @brief 複数キャッシュの比較バーグラフを生成
     * @param cacheIds キャッシュIDのリスト
     * @param metrics 測定指標のリスト（"hitRate", "accessTime", "invalidations"など）
     * @param config グラフ設定
     * @return グラフを表すHTML文字列
     */
    std::string GenerateComparisonBarGraph(
        const std::vector<std::string>& cacheIds,
        const std::vector<std::string>& metrics,
        const ICGraphConfig& config = ICGraphConfig());

    /**
     * @brief グラフをファイルに保存
     * @param htmlContent グラフのHTML内容
     * @param filePath 保存先ファイルパス
     * @param format エクスポート形式
     * @return 成功した場合はtrue
     */
    bool SaveGraphToFile(
        const std::string& htmlContent,
        const std::string& filePath,
        ICVisualizationExportFormat format = ICVisualizationExportFormat::HTML);

    /**
     * @brief キャッシュパフォーマンスダッシュボードのHTMLを生成
     * @param includeAllCaches すべてのキャッシュを含めるかどうか
     * @param timeRange 時間範囲（ミリ秒）、0の場合は全履歴
     * @return ダッシュボードを表すHTML文字列
     */
    std::string GenerateDashboard(bool includeAllCaches = true, uint64_t timeRange = 0);

    /**
     * @brief グラフをIDで保存
     * @param graphId 保存するグラフのID
     * @param graphType グラフタイプ
     * @param htmlContent グラフのHTML内容
     * @param config グラフ設定
     * @param series データ系列
     * @param cacheId 関連するキャッシュID（該当する場合）
     * @return 成功した場合はtrue
     */
    bool SaveGraphById(
        const std::string& graphId,
        ICGraphType graphType,
        const std::string& htmlContent,
        const ICGraphConfig& config,
        const std::vector<ICGraphSeries>& series,
        const std::string& cacheId = "");

    /**
     * @brief 保存されたグラフを取得
     * @param graphId グラフID
     * @return 保存されたグラフ設定、見つからない場合は空のオプション
     */
    std::optional<ICSavedGraph> GetSavedGraph(const std::string& graphId) const;

    /**
     * @brief すべての保存されたグラフIDを取得
     * @return 保存されたグラフIDのリスト
     */
    std::vector<std::string> GetAllSavedGraphIds() const;

    /**
     * @brief 保存されたグラフを削除
     * @param graphId グラフID
     * @return 成功した場合はtrue
     */
    bool DeleteSavedGraph(const std::string& graphId);

    /**
     * @brief グラフのカラーテーマを設定
     * @param theme カラーテーマ
     */
    void SetColorTheme(ICGraphColorTheme theme);

    /**
     * @brief デフォルトのグラフサイズを設定
     * @param width 幅（ピクセル）
     * @param height 高さ（ピクセル）
     */
    void SetDefaultGraphSize(int width, int height);

    /**
     * @brief カスタムCSSスタイルを設定
     * @param cssStyles CSSスタイル文字列
     */
    void SetCustomStyles(const std::string& cssStyles);

    /**
     * @brief グラフのレンダリングエンジンを設定
     * @param engineName エンジン名（"d3", "plotly", "chart.js"など）
     */
    void SetRenderingEngine(const std::string& engineName);

    /**
     * @brief リアルタイムグラフ更新の有効/無効を設定
     * @param enabled 有効にする場合はtrue
     * @param updateIntervalMs 更新間隔（ミリ秒）
     */
    void EnableRealtimeUpdates(bool enabled, uint64_t updateIntervalMs = 1000);

private:
    /**
     * @brief プライベートコンストラクタ（シングルトンパターン）
     */
    ICVisualizer();

    // コピーと代入を禁止
    ICVisualizer(const ICVisualizer&) = delete;
    ICVisualizer& operator=(const ICVisualizer&) = delete;

    /**
     * @brief グラフ用の共通JavaScriptライブラリをインクルード
     * @return ライブラリをインクルードするHTML文字列
     */
    std::string IncludeGraphLibraries() const;

    /**
     * @brief カラーテーマに基づいてカラーマップを取得
     * @param theme カラーテーマ
     * @return カラーコードのベクトル
     */
    std::vector<std::string> GetColorMap(ICGraphColorTheme theme) const;

    /**
     * @brief 時系列データを準備
     * @param cacheIds キャッシュIDのリスト
     * @param metric 測定指標（"hitRate", "accessTime"など）
     * @param timeRange 時間範囲（ミリ秒）
     * @return データ系列のベクトル
     */
    std::vector<ICGraphSeries> PrepareTimeSeriesData(
        const std::vector<std::string>& cacheIds,
        const std::string& metric,
        uint64_t timeRange) const;

    /**
     * @brief 円グラフデータを準備
     * @param labels ラベルのベクトル
     * @param values 値のベクトル
     * @param seriesName 系列名
     * @return 円グラフ用データ系列
     */
    ICGraphSeries PreparePieChartData(
        const std::vector<std::string>& labels,
        const std::vector<double>& values,
        const std::string& seriesName) const;

    /**
     * @brief エンジン固有のグラフレンダリングを実行
     * @param graphType グラフタイプ
     * @param series データ系列
     * @param config グラフ設定
     * @return レンダリングされたグラフHTML
     */
    std::string RenderGraphWithEngine(
        ICGraphType graphType,
        const std::vector<ICGraphSeries>& series,
        const ICGraphConfig& config) const;

    /**
     * @brief D3.jsでグラフをレンダリング
     * @param graphType グラフタイプ
     * @param series データ系列
     * @param config グラフ設定
     * @return レンダリングされたグラフHTML
     */
    std::string RenderWithD3(
        ICGraphType graphType,
        const std::vector<ICGraphSeries>& series,
        const ICGraphConfig& config) const;

    /**
     * @brief Plotly.jsでグラフをレンダリング
     * @param graphType グラフタイプ
     * @param series データ系列
     * @param config グラフ設定
     * @return レンダリングされたグラフHTML
     */
    std::string RenderWithPlotly(
        ICGraphType graphType,
        const std::vector<ICGraphSeries>& series,
        const ICGraphConfig& config) const;

    /**
     * @brief Chart.jsでグラフをレンダリング
     * @param graphType グラフタイプ
     * @param series データ系列
     * @param config グラフ設定
     * @return レンダリングされたグラフHTML
     */
    std::string RenderWithChartJs(
        ICGraphType graphType,
        const std::vector<ICGraphSeries>& series,
        const ICGraphConfig& config) const;

    // メンバ変数
    std::string m_renderingEngine;                    // レンダリングエンジン名
    ICGraphColorTheme m_defaultColorTheme;            // デフォルトカラーテーマ
    int m_defaultWidth;                               // デフォルト幅
    int m_defaultHeight;                              // デフォルト高さ
    std::string m_customCssStyles;                    // カスタムCSSスタイル
    std::atomic<bool> m_realtimeUpdatesEnabled;       // リアルタイム更新の有効フラグ
    uint64_t m_realtimeUpdateInterval;                // リアルタイム更新間隔（ミリ秒）
    std::map<std::string, ICSavedGraph> m_savedGraphs; // 保存されたグラフのマップ
    mutable std::mutex m_graphsMutex;                 // グラフ操作用ミューテックス
};

} // namespace core
} // namespace aerojs 