# Endless Editor: SQL notebook with time-series visualization and live polling

## Summary

Endless Editor is a SQL-first workspace that combines a continuous query notebook, inline results, time-series visualization, and statistics.

Executed statements remain visible above one editable prompt, with their results, warnings, and errors attached. Users can inspect the same result as a data grid, chart, or statistical summary, and refresh read-only queries through polling.

This description reflects the current implementation, not a proposal or a promise of future functionality.

## Implemented features

### Connection management

- Saved connection tiles grouped by database engine.
- Add connection profiles through available connector plugins.
- Import SqlStudio / MySQL Workbench connection XML.
- Request the password when opening a connection; passwords are not saved to connection profiles.
- Display connection status and return to the connections workspace.

### Continuous SQL notebook

- One editable SQL prompt below read-only execution history.
- SQL syntax highlighting in both the prompt and executed statements.
- Execute with **Ctrl+Enter** or **Cmd+Enter**.
- Recall previous statements with **Up / Down**, preserving the unfinished draft.
- Continue drafting while a query is running; concurrent execution is disabled.
- Clear notebook history without resetting the database session.
- Preserve engine-supported session state across executions through one physical connection per session.

### Inline results

- Dynamic result tables with column names and database types.
- Explicit rendering of NULL values and binary-size placeholders.
- Command summaries, execution timing, warnings, and statement-level errors.
- Multiple result sets returned by stored procedures.
- Loading placeholders and result-truncation notices.

### Result views and charts

Each result set offers **Data Grid**, **Time Series**, **Statistics**, and **Visualize** tabs.

- Automatically detect timestamp and numeric columns.
- Manually select the timestamp column, measurements, and optional grouping column.
- Split measurements into series by device, symbol, or category.
- Render line, area, bar, scatter, and histogram charts.
- Configure dual Y axes, series visibility, and Y-axis bounds.
- Zoom, pan, reset zoom, and enter full screen.
- Export charts as PNG.
- Save visualization preferences locally per connection and query.

### Time-series processing

- Select range presets: 5 minutes, 15 minutes, 1 hour, 6 hours, 24 hours, 7 days, or all available data.
- Range presets are relative to the latest sample in the dataset.
- Aggregate by average, sum, minimum, maximum, or count at a selected interval.
- Choose browser-side aggregation or database-side bucketing where supported.
- Downsample using min/max to preserve spikes or LTTB to preserve shape.
- Configure rendered-point and retained-sample limits.
- Configure local or UTC interpretation of naive timestamps and the display time zone.

### Live query controls

- Refresh read-only queries manually or through polling.
- Start, pause, resume, stop, and refresh live views.
- Cancel a running visualization query when supported by the connector.
- Fetch new rows incrementally where supported.
- Show live status, last update time, rendered-point counts, and truncation notices.
- Prevent overlapping polling requests and bound retained samples.
- Keep existing charts and statistics visible during refresh.

### Statistics

- Latest-value cards for visible series.
- Per-series minimum, maximum, average, and standard deviation.
- Sample and null counts.
- First and last values and timestamps.
- Sample rate and selected-range summary.
- Statistics calculated from the selected, aggregated dataset before rendering downsampling.

## Shipped connectors

| Database | Driver / transport | Time-series charts | Polling | Incremental fetch | Server bucketing | Cancellation |
| --- | --- | --- | --- | --- | --- | --- |
| MySQL / MariaDB | PyMySQL | Yes | Yes | Yes | Yes | Yes |
| PostgreSQL | psycopg / PGWire | Yes | Yes | Yes | Yes | Yes |
| QuestDB | psycopg / PGWire | Yes | Yes | Yes | Yes | Yes |

- MySQL and MariaDB share one connector plugin.
- QuestDB uses PGWire, with default port **8812**, not HTTP console port 9000.
- Driver availability determines whether a connector can be selected.
- Engine logos do not imply connector support. SQL Server, InfluxDB, TimescaleDB, and ClickHouse watermarks are not dedicated connectors.
- None of the shipped plugins provides native query streaming.
- PostgreSQL connector capabilities are implemented, but a live PostgreSQL round trip has not yet been verified in the documented checks.

## Typical workflow

1. Select a saved connection or add a supported database profile.
2. Enter the connection password.
3. Execute one SQL statement and inspect its inline results.
4. Open **Time Series**, **Statistics**, or **Visualize**.
5. Accept detected columns or configure the timestamp, measurements, and grouping.
6. Adjust the range, aggregation, axes, and rendering settings.
7. Enable polling for updated measurements or export the chart as PNG.

## Architecture and delivery

- Shared React / TypeScript frontend with a separate FastAPI backend.
- Native desktop hosts:
  - **Windows:** WinForms with WebView2.
  - **macOS:** AppKit with WKWebView.
  - **Linux:** GTK 4 with WebKitGTK.
- Windows build and startup have been verified; macOS and Linux require platform verification.
- Backend and database processes run separately from the native host.
- Commands are serialized within each database session.
- Default retained grid limit: **1,000 rows per result set**.
- Default time-series fetch limit: **100,000 rows**.
- Backend row limits are configurable; they do not bound database execution cost.

## Current limitations / out of scope

- One SQL statement per execution; no semicolon-separated scripts or client `DELIMITER` commands.
- No multiple notebook tabs.
- Execution history is in memory and resets on page reload.
- No durable history, paging, virtualized data grid, or schema browser.
- Syntax highlighting does not provide SQL validation or schema autocomplete.
- No editing of saved connection profiles, SSL configuration UI, or OS credential storage.
- No native streaming transport.
- No heatmaps, state timelines, event annotations, or gauges.
- Session SQL behavior depends on the database; QuestDB does not provide PostgreSQL's full transaction semantics.
- The API is a local developer service without authentication and should run on loopback.

## Implementation reference

Feature snapshot: **4 November 2026**. Capabilities above describe the current code; implementation support should not be interpreted as live verification on every database or operating system.
