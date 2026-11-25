// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"

#include <QEventLoop>
#include <QSet>

using namespace MainWindowTestHelpers;

#ifdef Q_OS_MACOS
void* createDockTestAutoreleasePool();
void drainDockTestAutoreleasePool(void* pool);
#endif

namespace {

bool selectDockTab(QMainWindow& window, QDockWidget* dock)
{
    for (QTabBar* bar : window.findChildren<QTabBar*>(QString(), Qt::FindDirectChildrenOnly)) {
        for (int i = 0; i < bar->count(); ++i) {
            if (bar->tabText(i) == dock->windowTitle()) {
                QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(i).center());
                return bar->currentIndex() == i;
            }
        }
    }
    return false;
}

// isFullScreen() changes as soon as fullscreen is requested, but a macOS
// fullscreen animation ends later. Qt ignores showNormal() until it ends, and
// then reports the window as fullscreen again. Track what the platform
// reports, so the test waits for the animation instead of a fixed time.
class ReportedWindowState : public QObject {
public:
    explicit ReportedWindowState(QWidget* window) : m_window(window)
    {
#ifdef Q_OS_MACOS
        qApp->installEventFilter(this);
#endif
    }

    bool isFullScreen() const
    {
#ifdef Q_OS_MACOS
        return m_states.testFlag(Qt::WindowFullScreen);
#else
        return m_window->isFullScreen();
#endif
    }

    template<class Request>
    bool changeFullScreen(bool fullscreen, Request request)
    {
        constexpr int timeout = 15000;
#ifdef Q_OS_MACOS
        // Run the request and wait in AppKit's normal event loop.
        QEventLoop loop;
        m_waitLoop = &loop;
        m_expectedFullScreen = fullscreen;
        QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        QTimer::singleShot(0, &loop, [&]() {
            request();
            if (isFullScreen() == fullscreen)
                loop.quit();
        });
        loop.exec();
        m_waitLoop = nullptr;
        return isFullScreen() == fullscreen;
#else
        request();
        return QTest::qWaitFor([&]() { return isFullScreen() == fullscreen; }, timeout);
#endif
    }

#ifdef Q_OS_MACOS
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Only the platform sends spontaneous state changes to the QWindow.
        QWindow* handle = m_window->windowHandle();
        if (handle != nullptr && watched == handle && event->type() == QEvent::WindowStateChange
            && event->spontaneous()) {
            m_states = handle->windowStates();
            if (m_waitLoop != nullptr && isFullScreen() == m_expectedFullScreen)
                m_waitLoop->quit();
        }
        return false;
    }
#endif

private:
    QWidget* m_window;
#ifdef Q_OS_MACOS
    Qt::WindowStates m_states = Qt::WindowNoState;
    QEventLoop* m_waitLoop = nullptr;
    bool m_expectedFullScreen = false;
#endif
};

} // namespace

#ifdef Q_OS_WIN
namespace {

// Pads list rows but not tree rows, as the Windows 11 style does.
class ListRowPaddingTestStyle : public QProxyStyle {
public:
    ListRowPaddingTestStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {}

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_ItemViewItem && qobject_cast<const QListView*>(widget))
            result += QSize(6, 12);
        return result;
    }

    QRect subElementRect(SubElement element, const QStyleOption* option,
                         const QWidget* widget = nullptr) const override
    {
        QRect result = QProxyStyle::subElementRect(element, option, widget);
        if (element == SE_ItemViewItemText && qobject_cast<const QListView*>(widget))
            result.translate(6, 0);
        return result;
    }
};

int firstPaintedColumn(QAbstractItemView* view)
{
    const QImage image = view->viewport()->grab().toImage();
    const QRgb background = image.pixel(image.width() - 1, image.height() - 1);
    for (int x = 0; x < image.width(); ++x) {
        for (int y = 0; y < image.height(); ++y) {
            if (image.pixel(x, y) != background)
                return x;
        }
    }
    return -1;
}

} // namespace
#endif

class TestDockLayoutUi : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
#ifdef Q_OS_MACOS
    void cleanup();
#endif
    void lazy_docks_stay_uncreated_until_opened();
    void lazy_docks_restore_saved_layouts_data();
    void lazy_docks_restore_saved_layouts();
    void lazy_docks_restore_each_windows_open_panels();
    void opening_docks_keeps_floating_panels_independent();
    void floating_docks_survive_repeated_quit_data();
    void floating_docks_survive_repeated_quit();
    void dock_sizes_survive_relaunch_data();
    void dock_sizes_survive_relaunch();
    void fullscreen_dock_widths_survive_relaunch_data();
    void fullscreen_dock_widths_survive_relaunch();
    void dock_list_selected_row_keeps_primary_fill_while_hovered();
    void functions_dock_retranslates_domain_label_after_language_change();
    void functions_dock_finds_geometry_by_name();
    void side_docks_keep_full_height_data();
    void side_docks_keep_full_height();
    void dock_search_rows_use_configured_vertical_padding_data();
    void dock_search_rows_use_configured_vertical_padding();
    void themed_dock_controls_have_equal_visible_gaps_data();
    void themed_dock_controls_have_equal_visible_gaps();
    void themed_dock_lists_use_compact_spacing_data();
    void themed_dock_lists_use_compact_spacing();
#ifdef Q_OS_WIN
    void dock_list_rows_match_tree_rows_data();
    void dock_list_rows_match_tree_rows();
#endif
    void dock_first_control_matches_shared_vertical_spacing_data();
    void dock_first_control_matches_shared_vertical_spacing();
    void formula_book_text_scales_with_zoom_data();
    void formula_book_text_scales_with_zoom();
    void formula_book_links_follow_dock_appearance_data();
    void formula_book_links_follow_dock_appearance();
    void formula_book_ignores_repeated_appearance_events_data();
    void formula_book_ignores_repeated_appearance_events();
    void formula_book_combines_palette_changes();
    void formula_book_text_requests_single_font_family();
    void formula_book_covers_geometry_catalog();
    void formula_book_inserts_geometry_function_calls();
    void functions_dock_lists_cbrt_without_its_symbol();
    void startup_styling_keeps_formula_book_document();
    void formula_book_retranslation_refreshes_cached_page();
    void restored_constants_dock_empty_filter_fills_header();
    void dock_scroll_corner_uses_scrollbar_track_fill();
    void dock_separator_style_uses_primary_while_hovered_or_dragged();
    void constants_dock_uses_configured_narrow_minimum_width();
    void lone_side_dock_opens_wide_enough_for_columns_data();
    void lone_side_dock_opens_wide_enough_for_columns();
    void lone_formula_book_opens_wide_enough_for_page();
    void kept_window_size_handles_native_resize_requests_data();
    void kept_window_size_handles_native_resize_requests();
    void shown_dock_keeps_window_size_from_window_system();
    void fitted_and_kept_dock_widths_apply_at_once();
    void view_dock_menu_tracks_and_changes_only_active_window();
    void closing_and_reopening_docks_keeps_attached_widgets();
    void dock_column_orders_survive_relaunch_data();
    void dock_column_orders_survive_relaunch();
    void dock_column_orders_are_per_window();
    void invalid_dock_column_orders_use_defaults_data();
    void invalid_dock_column_orders_use_defaults();
    void dock_lists_wait_until_exposed_data();
    void dock_lists_wait_until_exposed();
    void covered_functions_keep_pending_filters_and_language();
    void covered_history_keeps_pending_session_and_format();
    void restored_dock_lists_wait_behind_tabs_data();
    void restored_dock_lists_wait_behind_tabs();
    void reopened_dock_lists_refresh_with_retained_geometry_data();
    void reopened_dock_lists_refresh_with_retained_geometry();

#ifdef Q_OS_MACOS
private:
    void* m_autoreleasePool = nullptr;
#endif
};

void TestDockLayoutUi::init()
{
#ifdef Q_OS_MACOS
    // Release Cocoa objects before the next test creates more windows.
    m_autoreleasePool = createDockTestAutoreleasePool();
#endif
    UiTestFixture::resetSettings();
}

#ifdef Q_OS_MACOS
void TestDockLayoutUi::cleanup()
{
    drainDockTestAutoreleasePool(m_autoreleasePool);
    m_autoreleasePool = nullptr;
}
#endif

void TestDockLayoutUi::reopened_dock_lists_refresh_with_retained_geometry_data()
{
    QTest::addColumn<int>("kind");
    const QStringList names {QStringLiteral("constants"), QStringLiteral("functions"),
        QStringLiteral("history"), QStringLiteral("variables"),
        QStringLiteral("user-functions"), QStringLiteral("user-units")};
    for (int kind = 0; kind < names.size(); ++kind)
        QTest::newRow(qPrintable(names.at(kind))) << kind;
}

void TestDockLayoutUi::reopened_dock_lists_refresh_with_retained_geometry()
{
    QFETCH(int, kind);
    MainWindowStateGuard guard;
    MainWindow::initializeApplicationStyle();
    Session session;
    session.addHistoryEntry(HistoryEntry(QStringLiteral("10000"), Quantity(10000)));
    session.addVariable(Variable(QStringLiteral("dockvalue"), Quantity(42)));
    session.addUserFunction(UserFunction(QStringLiteral("dockfunction"),
                                        {QStringLiteral("x")}, QStringLiteral("x+1")));
    session.addUserUnit(UserUnit(QStringLiteral("dockunit"), Quantity(2)));
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("Editor")));
    window.resize(800, 600);
    auto* dock = new QDockWidget(QStringLiteral("List"), &window);
    QWidget* content = nullptr;
    switch (kind) {
    case 0:
        content = new ConstantsWidget(dock);
        break;
    case 1:
        content = new FunctionsWidget(dock);
        break;
    case 2: {
        auto* history = new HistoryWidget(dock);
        history->setSession(&session);
        content = history;
        break;
    }
    case 3: {
        auto* variables = new VariableListWidget(dock);
        variables->setEvaluator(session.evaluator());
        content = variables;
        break;
    }
    case 4: {
        auto* functions = new UserFunctionListWidget(dock);
        functions->setEvaluator(session.evaluator());
        content = functions;
        break;
    }
    case 5: {
        auto* units = new UserUnitListWidget(dock);
        units->setEvaluator(session.evaluator());
        content = units;
        break;
    }
    }
    QVERIFY(content);
    dock->setWidget(content);
    auto* covering = new QDockWidget(QStringLiteral("Cover"), &window);
    covering->setWidget(new QLabel(QStringLiteral("Cover")));
    window.addDockWidget(Qt::LeftDockWidgetArea, dock);
    window.addDockWidget(Qt::LeftDockWidgetArea, covering);
    window.tabifyDockWidget(dock, covering);
    covering->raise();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const auto rowCount = [content]() {
        if (auto* tree = content->findChild<QTreeWidget*>())
            return tree->topLevelItemCount();
        return content->findChild<QListWidget*>()->count();
    };
    QCOMPARE(rowCount(), 0);

    dock->hide();
    dock->show();
    // Reproduce a covered dock that still has an on-screen rectangle during
    // a layout change. Qt can then switch tabs without a visibility signal.
    dock->setGeometry(covering->geometry());
    covering->raise();
    QVERIFY(DockListStyle::shouldDeferPopulation(content));
    QCOMPARE(rowCount(), 0);
    QVERIFY(selectDockTab(window, dock));
    QVERIFY(!DockListStyle::shouldDeferPopulation(content));
    // Rows must be ready before the next paint, without an event-loop wait.
    QVERIFY(rowCount() > 0);
}

void TestDockLayoutUi::dock_lists_wait_until_exposed_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("exposure");
    const QStringList names {QStringLiteral("constants"), QStringLiteral("functions"),
        QStringLiteral("history"), QStringLiteral("variables"),
        QStringLiteral("user-functions"), QStringLiteral("user-units")};
    const QStringList exposures {QStringLiteral("tab"), QStringLiteral("float"), QStringLiteral("reopen")};
    for (int kind = 0; kind < names.size(); ++kind) {
        for (int exposure = 0; exposure < exposures.size(); ++exposure)
            QTest::newRow(qPrintable(names.at(kind) + QLatin1Char('-') + exposures.at(exposure)))
                << kind << exposure;
    }
}

void TestDockLayoutUi::dock_lists_wait_until_exposed()
{
    QFETCH(int, kind);
    QFETCH(int, exposure);
    MainWindowStateGuard guard;
    MainWindow::initializeApplicationStyle();
    Session session;
    session.addHistoryEntry(HistoryEntry(QStringLiteral("10000"), Quantity(10000)));
    session.addVariable(Variable(QStringLiteral("dockvalue"), Quantity(42)));
    session.addUserFunction(UserFunction(QStringLiteral("dockfunction"),
                                        {QStringLiteral("x")}, QStringLiteral("x+1")));
    session.addUserUnit(UserUnit(QStringLiteral("dockunit"), Quantity(2)));
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("Editor")));
    window.resize(800, 600);
    auto* dock = new QDockWidget(QStringLiteral("List"), &window);
    QWidget* content = nullptr;
    switch (kind) {
    case 0:
        content = new ConstantsWidget(dock);
        break;
    case 1:
        content = new FunctionsWidget(dock);
        break;
    case 2: {
        auto* history = new HistoryWidget(dock);
        history->setSession(&session);
        content = history;
        break;
    }
    case 3: {
        auto* variables = new VariableListWidget(dock);
        variables->setEvaluator(session.evaluator());
        content = variables;
        break;
    }
    case 4: {
        auto* functions = new UserFunctionListWidget(dock);
        functions->setEvaluator(session.evaluator());
        content = functions;
        break;
    }
    case 5: {
        auto* units = new UserUnitListWidget(dock);
        units->setEvaluator(session.evaluator());
        content = units;
        break;
    }
    }
    QVERIFY(content);
    dock->setWidget(content);
    auto* covering = new QDockWidget(QStringLiteral("Cover"), &window);
    covering->setWidget(new QLabel(QStringLiteral("Cover")));
    window.addDockWidget(Qt::LeftDockWidgetArea, dock);
    window.addDockWidget(Qt::LeftDockWidgetArea, covering);
    window.tabifyDockWidget(dock, covering);
    covering->raise();
    const auto rowCount = [content]() {
        if (auto* tree = content->findChild<QTreeWidget*>())
            return tree->topLevelItemCount();
        return content->findChild<QListWidget*>()->count();
    };
    QCOMPARE(rowCount(), 0);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();
    QVERIFY(!covering->visibleRegion().isEmpty());
    QVERIFY(dock->visibleRegion().isEmpty());
    QCOMPARE(rowCount(), 0);

    if (exposure == 1) {
        dock->setFloating(true);
        dock->resize(300, 300);
        dock->move(window.mapToGlobal(QPoint(40, 40)));
    } else {
        if (exposure == 2) {
            dock->hide();
            dock->show();
            QVERIFY(selectDockTab(window, dock));
        } else {
            QVERIFY(selectDockTab(window, dock));
        }
    }
    // Populate during activation, before the next paint or filter timer.
    QVERIFY(rowCount() > 0);
    auto* view = content->findChild<QAbstractItemView*>();
    QVERIFY(view);
    QTRY_VERIFY(!view->viewport()->visibleRegion().isEmpty());

    if (dock->isFloating()) {
        dock->setFloating(false);
        window.tabifyDockWidget(dock, covering);
    }
    QVERIFY(selectDockTab(window, covering));
    QVERIFY(dock->visibleRegion().isEmpty());
    const int previousCount = rowCount();
    const QString noMatch = QStringLiteral("no-matching-dock-entry");
    if (auto* constants = qobject_cast<ConstantsWidget*>(content))
        constants->restoreState(constants->selectedDomain(), constants->selectedSubdomain(), noMatch);
    else if (auto* functions = qobject_cast<FunctionsWidget*>(content))
        functions->setSearchText(noMatch);
    else if (auto* variables = qobject_cast<VariableListWidget*>(content))
        variables->setSearchText(noMatch);
    else if (auto* userFunctions = qobject_cast<UserFunctionListWidget*>(content))
        userFunctions->setSearchText(noMatch);
    else if (auto* units = qobject_cast<UserUnitListWidget*>(content))
        units->setSearchText(noMatch);
    else {
        session.addHistoryEntry(HistoryEntry(QStringLiteral("20000"), Quantity(20000)));
        qobject_cast<HistoryWidget*>(content)->updateHistory();
    }
    QCOMPARE(rowCount(), previousCount);
    QVERIFY(selectDockTab(window, dock));
    QCOMPARE(rowCount(), kind == 2 ? previousCount + 1 : 0);
}

void TestDockLayoutUi::covered_functions_keep_pending_filters_and_language()
{
    MainWindowStateGuard guard;
    MainWindow::initializeApplicationStyle();
    QMainWindow window;
    window.resize(800, 600);
    auto* dock = new QDockWidget(QStringLiteral("Functions"), &window);
    auto* functions = new FunctionsWidget(dock, {QStringLiteral("All"), QStringLiteral("sin")});
    dock->setWidget(functions);
    auto* covering = new QDockWidget(QStringLiteral("Cover"), &window);
    covering->setWidget(new QLabel(QStringLiteral("Cover")));
    window.addDockWidget(Qt::LeftDockWidgetArea, dock);
    window.addDockWidget(Qt::LeftDockWidgetArea, covering);
    window.tabifyDockWidget(dock, covering);
    covering->raise();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* list = functions->findChild<QTreeWidget*>();
    QCOMPARE(list->topLevelItemCount(), 0);
    FunctionsTestTranslator translator;
    QCoreApplication::installTranslator(&translator);
    const auto removeTranslator = qScopeGuard([&]() { QCoreApplication::removeTranslator(&translator); });
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(functions, &languageChange);
    bool translatedDomain = false;
    for (const auto* label : functions->findChildren<QLabel*>())
        translatedDomain |= label->text() == QStringLiteral("Translated Domain");
    QVERIFY(translatedDomain);
    functions->setSelectedDomain(FunctionRepo::instance()->find(QStringLiteral("cbrt"))->domain());
    functions->setSearchText(QStringLiteral("cbrt"));
    QCoreApplication::processEvents();
    QCOMPARE(list->topLevelItemCount(), 0);
    QVERIFY(selectDockTab(window, dock));
    QCOMPARE(list->topLevelItemCount(), 1);
    QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("cbrt"));
    QCOMPARE(functions->searchText(), QStringLiteral("cbrt"));

    // Once populated, leave covered rows alone and refresh the latest filter
    // immediately when returning, even before its usual debounce interval.
    QVERIFY(selectDockTab(window, covering));
    functions->setSearchText(QStringLiteral("sqrt"));
    QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("cbrt"));
    QVERIFY(selectDockTab(window, dock));
    QCOMPARE(list->topLevelItemCount(), 1);
    QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("sqrt"));
}

void TestDockLayoutUi::covered_history_keeps_pending_session_and_format()
{
    MainWindowStateGuard guard;
    MainWindow::initializeApplicationStyle();
    guard.settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    guard.settings->applyNumberFormatStyle();
    Session first;
    first.addHistoryEntry(HistoryEntry(QStringLiteral("10000"), Quantity(10000)));
    Session second;
    second.addHistoryEntry(HistoryEntry(QStringLiteral("20000"), Quantity(20000)));
    HistoryWidget standalone;
    standalone.setSession(&first);
    QCOMPARE(standalone.findChild<QListWidget*>()->count(), 1);
    QMainWindow window;
    window.resize(800, 600);
    auto* dock = new QDockWidget(QStringLiteral("History"), &window);
    auto* history = new HistoryWidget(dock);
    history->setSession(&first);
    dock->setWidget(history);
    auto* covering = new QDockWidget(QStringLiteral("Cover"), &window);
    covering->setWidget(new QLabel(QStringLiteral("Cover")));
    window.addDockWidget(Qt::LeftDockWidgetArea, dock);
    window.addDockWidget(Qt::LeftDockWidgetArea, covering);
    window.tabifyDockWidget(dock, covering);
    covering->raise();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* list = history->findChild<QListWidget*>();
    QCOMPARE(list->count(), 0);
    history->setSession(&second);
    second.addHistoryEntry(HistoryEntry(QStringLiteral("30000"), Quantity(30000)));
    history->updateHistory();
    guard.settings->numberFormatStyle = Settings::NumberFormatThreeDigitCommaDot;
    guard.settings->applyNumberFormatStyle();
    history->updateHistory();
    QCOMPARE(list->count(), 0);
    QVERIFY(selectDockTab(window, dock));
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->text(), QStringLiteral("20,000"));
    QCOMPARE(list->item(1)->text(), QStringLiteral("30,000"));
    QCOMPARE(list->item(0)->data(Qt::UserRole).toString(), QStringLiteral("20000"));

    QVERIFY(selectDockTab(window, covering));
    second.clearHistory();
    second.addHistoryEntry(HistoryEntry(QStringLiteral("40000"), Quantity(40000)));
    history->updateHistory();
    QCOMPARE(list->count(), 2);
    QVERIFY(selectDockTab(window, dock));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("40,000"));
}

void TestDockLayoutUi::restored_dock_lists_wait_behind_tabs_data()
{
    QTest::addColumn<bool>("themed");
    QTest::newRow("system") << false;
    QTest::newRow("themed") << true;
}

void TestDockLayoutUi::restored_dock_lists_wait_behind_tabs()
{
    QFETCH(bool, themed);
    MainWindowStateGuard guard;
    guard.settings->dockAppearance = themed ? Settings::DockAppearanceSpeedCrunch : Settings::DockAppearanceSystem;
    guard.settings->constantsDockVisible = true;
    guard.settings->functionsDockVisible = true;
    guard.settings->historyDockVisible = true;
    guard.settings->variablesDockVisible = true;
    guard.settings->userFunctionsDockVisible = true;
    guard.settings->userUnitsDockVisible = true;
    guard.settings->bitfieldVisible = false;
    guard.settings->formulaBookDockVisible = false;
    guard.settings->windowState.clear();
    guard.settings->sessionLayoutJson.clear();
    MainWindow window(false);
    auto* functions = window.findChild<FunctionsWidget*>();
    auto* constants = window.findChild<ConstantsWidget*>();
    auto* history = window.findChild<HistoryWidget*>();
    QVERIFY(functions);
    QVERIFY(constants);
    QVERIFY(history);
    window.findChild<Editor*>()->setText(QStringLiteral("42"));
    QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();
    // User Units is the last created dock, above the other list tabs.
    for (QWidget* content : {static_cast<QWidget*>(constants), static_cast<QWidget*>(functions),
                             static_cast<QWidget*>(history)}) {
        auto* dock = qobject_cast<QDockWidget*>(content->parentWidget());
        QVERIFY(dock);
        QVERIFY(dock->visibleRegion().isEmpty());
        if (auto* tree = content->findChild<QTreeWidget*>())
            QCOMPARE(tree->topLevelItemCount(), 0);
        else
            QCOMPARE(content->findChild<QListWidget*>()->count(), 0);
    }
    qobject_cast<QDockWidget*>(functions->parentWidget())->raise();
    QVERIFY(functions->findChild<QTreeWidget*>()->topLevelItemCount() > 0);
    qobject_cast<QDockWidget*>(history->parentWidget())->raise();
    QCOMPARE(history->findChild<QListWidget*>()->count(), 1);
}

void TestDockLayoutUi::dock_column_orders_survive_relaunch_data()
{
    QTest::addColumn<bool>("workspaceLayout");
    QTest::addColumn<bool>("hidden");
    QTest::newRow("open-workspace") << true << false;
    QTest::newRow("hidden-workspace") << true << true;
    QTest::newRow("open-settings") << false << false;
    QTest::newRow("hidden-settings") << false << true;
}

void TestDockLayoutUi::dock_column_orders_survive_relaunch()
{
    QFETCH(bool, workspaceLayout);
    QFETCH(bool, hidden);
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    const QStringList names {QStringLiteral("ConstantsDock"), QStringLiteral("FunctionsDock"),
        QStringLiteral("VariablesDock"), QStringLiteral("UserFunctionsDock"), QStringLiteral("UserUnitsDock")};
    const QList<QByteArray> setters {"setConstantsDockVisible", "setFunctionsDockVisible",
        "setVariablesDockVisible", "setUserFunctionsDockVisible", "setUserUnitsDockVisible"};
    for (int launch = 0; launch < 3; ++launch) {
        if (launch != 0) {
            guard.settings->load();
            if (!workspaceLayout)
                guard.settings->sessionLayoutJson.clear();
        }
        MainWindow window(launch != 0);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Save once without ever opening the hidden panels on this launch.
        if (hidden && launch == 1) {
            QCOMPARE(window.findChildren<QDockWidget*>().size(), 0);
        } else {
            for (int i = 0; i < names.size(); ++i) {
                QVERIFY(QMetaObject::invokeMethod(&window, setters.at(i).constData(), Qt::DirectConnection,
                                                 Q_ARG(bool, true), Q_ARG(bool, false)));
                QDockWidget* dock = window.findChild<QDockWidget*>(names.at(i));
                QVERIFY(dock);
                QTreeWidget* table = dock->findChild<QTreeWidget*>();
                QVERIFY(table);
                QHeaderView* header = table->header();
                if (launch == 0) {
                    for (int visual = 0; visual < header->count(); ++visual)
                        QCOMPARE(header->logicalIndex(visual), visual);
                    header->moveSection(header->count() - 1, 0);
                    QEvent languageChange(QEvent::LanguageChange);
                    QCoreApplication::sendEvent(table->parentWidget(), &languageChange);
                    QCoreApplication::processEvents();
                }
                for (int visual = 0; visual < header->count(); ++visual)
                    QCOMPARE(header->logicalIndex(visual), (visual + header->count() - 1) % header->count());
                if (hidden)
                    dock->hide();
            }
        }
        window.persistSessionAndSettingsForShutdown();
    }
}

void TestDockLayoutUi::dock_column_orders_are_per_window()
{
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    {
        MainWindow first(false);
        MainWindow second(false);
        for (MainWindow* window : {&first, &second}) {
            QVERIFY(QMetaObject::invokeMethod(window, "setVariablesDockVisible", Qt::DirectConnection,
                                             Q_ARG(bool, true), Q_ARG(bool, false)));
            QHeaderView* header = window->findChild<VariableListWidget*>()->findChild<QTreeWidget*>()->header();
            header->moveSection(window == &first ? 2 : 1, 0);
            window->findChild<Editor*>()->setText(QStringLiteral("2+3"));
            QVERIFY(QMetaObject::invokeMethod(window, "evaluateEditorExpression", Qt::DirectConnection));
        }
        first.persistSessionAndSettingsForShutdown();
    }
    guard.settings->load();
    QJsonObject layout = QJsonDocument::fromJson(guard.settings->sessionLayoutJson.toUtf8()).object();
    const QJsonArray windows = layout.value(QStringLiteral("windows")).toArray();
    QCOMPARE(windows.size(), 2);
    const auto selectWindow = [&](int index) {
        const QJsonObject record = windows.at(index).toObject();
        layout.insert(QStringLiteral("windows"), QJsonArray{record});
        layout.insert(QStringLiteral("activeWindow"), record.value(QStringLiteral("id")));
        guard.settings->sessionLayoutJson = QString::fromUtf8(QJsonDocument(layout).toJson(QJsonDocument::Compact));
    };
    // Restore each record without relying on the once-per-process window spawn.
    selectWindow(0);
    MainWindow restored;
    selectWindow(1);
    MainWindow other;
    restored.show();
    other.show();
    for (MainWindow* window : {&restored, &other}) {
        QTRY_COMPARE(window->findChild<ResultDisplay*>()->session()->historySize(), 1);
        VariableListWidget* variables = window->findChild<VariableListWidget*>();
        QVERIFY(variables);
        QHeaderView* header = variables->findChild<QTreeWidget*>()->header();
        const QList<int> expected = window == &restored ? QList<int>{2, 0, 1} : QList<int>{1, 0, 2};
        for (int visual = 0; visual < expected.size(); ++visual)
            QCOMPARE(header->logicalIndex(visual), expected.at(visual));
    }
}

void TestDockLayoutUi::invalid_dock_column_orders_use_defaults_data()
{
    QTest::addColumn<QJsonArray>("order");
    QTest::newRow("duplicate") << QJsonArray{1, 1, 0};
    QTest::newRow("negative") << QJsonArray{2, -1, 0};
    QTest::newRow("out-of-range") << QJsonArray{2, 3, 0};
    QTest::newRow("missing-column") << QJsonArray{2, 0};
    QTest::newRow("extra-column") << QJsonArray{2, 0, 1, 3};
    QTest::newRow("text") << QJsonArray{2, QStringLiteral("1"), 0};
    QTest::newRow("fraction") << QJsonArray{2, 0.5, 0};
}

void TestDockLayoutUi::invalid_dock_column_orders_use_defaults()
{
    QFETCH(QJsonArray, order);
    Settings::instance()->dockColumnOrdersJson = QString::fromUtf8(QJsonDocument(
        QJsonObject{{QStringLiteral("VariablesDock"), order}}).toJson(QJsonDocument::Compact));
    MainWindow window(false);
    QVERIFY(QMetaObject::invokeMethod(&window, "setVariablesDockVisible", Qt::DirectConnection,
                                     Q_ARG(bool, true), Q_ARG(bool, false)));
    QTreeWidget* table = window.findChild<VariableListWidget*>()->findChild<QTreeWidget*>();
    for (int visual = 0; visual < table->columnCount(); ++visual)
        QCOMPARE(table->header()->logicalIndex(visual), visual);
}

void TestDockLayoutUi::initTestCase()
{
    // Fail before a file-operation test can open an unhandled error dialog.
    for (const QString& path : {Settings::getConfigPath(), Settings::getDataPath(),
                                Settings::getCachePath()}) {
        QVERIFY2(QDir().mkpath(path),
                 qPrintable(QStringLiteral("Cannot create test storage at %1").arg(path)));
        QTemporaryFile probe(QDir(path).filePath(QStringLiteral("storage-probe-XXXXXX")));
        QVERIFY2(probe.open(),
                 qPrintable(QStringLiteral("Cannot write test storage at %1: %2")
                                .arg(path, probe.errorString())));
    }
}

void TestDockLayoutUi::lazy_docks_stay_uncreated_until_opened()
{
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    MainWindow window(false);
    QCOMPARE(window.findChildren<QDockWidget*>().size(), 0);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor);
    editor->setText(QStringLiteral("6*7"));
    QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    QCOMPARE(window.findChild<ResultDisplay*>()->session()->historySize(), 1);
    QCOMPARE(window.findChildren<QDockWidget*>().size(), 0);
    const QStringList setters {QStringLiteral("setFormulaBookDockVisible"), QStringLiteral("setConstantsDockVisible"),
        QStringLiteral("setFunctionsDockVisible"), QStringLiteral("setHistoryDockVisible"),
        QStringLiteral("setVariablesDockVisible"), QStringLiteral("setUserFunctionsDockVisible"),
        QStringLiteral("setUserUnitsDockVisible"), QStringLiteral("setBitfieldVisible")};
    for (int i = 0; i < setters.size(); ++i) {
        const QByteArray setter = setters.at(i).toLatin1();
        const auto setVisible = [&](bool visible) {
            return i == 7
                ? QMetaObject::invokeMethod(&window, setter.constData(), Qt::DirectConnection, Q_ARG(bool, visible))
                : QMetaObject::invokeMethod(&window, setter.constData(), Qt::DirectConnection,
                                           Q_ARG(bool, visible), Q_ARG(bool, false));
        };
        QVERIFY(setVisible(true));
        QCoreApplication::processEvents();
        QCOMPARE(window.findChildren<QDockWidget*>().size(), i + 1);
        QVERIFY(setVisible(false));
        QVERIFY(setVisible(true));
        QCOMPARE(window.findChildren<QDockWidget*>().size(), i + 1);
        QVERIFY(setVisible(false));
    }
}

void TestDockLayoutUi::lazy_docks_restore_saved_layouts_data()
{
    QTest::addColumn<int>("format");
    QTest::newRow("workspace-open-dock-list") << 0;
    QTest::newRow("older-workspace") << 1;
    QTest::newRow("older-window-state") << 2;
}

void TestDockLayoutUi::lazy_docks_restore_saved_layouts()
{
    QFETCH(int, format);
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    QByteArray state;
    QString layout;
    QPoint floatingPosition;
    {
        MainWindow source(false);
        source.resize(960, 640);
        source.show();
        QVERIFY(QTest::qWaitForWindowExposed(&source));
        for (const char* setter : {"setConstantsDockVisible", "setFunctionsDockVisible", "setUserUnitsDockVisible"})
            QVERIFY(QMetaObject::invokeMethod(&source, setter, Qt::DirectConnection, Q_ARG(bool, true), Q_ARG(bool, false)));
        QDockWidget* units = source.findChild<QDockWidget*>(QStringLiteral("UserUnitsDock"));
        QVERIFY(units);
        units->setFloating(true);
        units->move(160, 120);
        units->resize(350, 280);
        floatingPosition = units->pos();
        units->hide();
        source.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"))->raise();
        QCoreApplication::processEvents();
        source.persistSessionAndSettingsForShutdown();
        state = source.saveState(1);
        layout = guard.settings->sessionLayoutJson;
    }
    UiTestFixture::resetSettings();
    guard.settings->windowState = state;
    if (format != 2) {
        if (format == 1) {
            QJsonObject saved = QJsonDocument::fromJson(layout.toUtf8()).object();
            QJsonArray windows = saved.value(QStringLiteral("windows")).toArray();
            QJsonObject window = windows.first().toObject();
            window.remove(QStringLiteral("openDocks"));
            windows[0] = window;
            saved.insert(QStringLiteral("windows"), windows);
            layout = QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact));
        }
        guard.settings->sessionLayoutJson = layout;
    }
    {
        MainWindow restored;
        restored.show();
        QVERIFY(QTest::qWaitForWindowExposed(&restored));
        QTRY_COMPARE(restored.findChildren<QDockWidget*>().size(), 2);
        QDockWidget* constants = restored.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
        QDockWidget* functions = restored.findChild<QDockWidget*>(QStringLiteral("FunctionsDock"));
        QVERIFY(constants && functions);
        QVERIFY(!constants->isHidden() && !functions->isHidden());
        QVERIFY(restored.tabifiedDockWidgets(constants).contains(functions));
        QTabBar* dockTabs = nullptr;
        for (QTabBar* bar : restored.findChildren<QTabBar*>()) {
            for (int i = 0; i < bar->count(); ++i) {
                if (bar->tabText(i) == constants->windowTitle())
                    dockTabs = bar;
            }
        }
        QVERIFY(dockTabs);
        QCOMPARE(dockTabs->tabText(dockTabs->currentIndex()), constants->windowTitle());
        QVERIFY(!restored.findChild<QDockWidget*>(QStringLiteral("UserUnitsDock")));
        // Saving before opening the absent dock must retain its saved placement.
        restored.persistSessionAndSettingsForShutdown();
    }
    {
        MainWindow reopened;
        reopened.show();
        QVERIFY(QTest::qWaitForWindowExposed(&reopened));
        QTRY_COMPARE(reopened.findChildren<QDockWidget*>().size(), 2);
        QVERIFY(QMetaObject::invokeMethod(&reopened, "setUserUnitsDockVisible", Qt::DirectConnection,
                                         Q_ARG(bool, true), Q_ARG(bool, false)));
        QDockWidget* units = reopened.findChild<QDockWidget*>(QStringLiteral("UserUnitsDock"));
        QVERIFY(units);
        QVERIFY(units->isFloating());
        QCOMPARE(units->pos(), floatingPosition);
        QVERIFY(units->findChild<QLineEdit*>());
        QCOMPARE(reopened.findChildren<QDockWidget*>().size(), 3);
    }
}

void TestDockLayoutUi::lazy_docks_restore_each_windows_open_panels()
{
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    {
        MainWindow first(false);
        QVERIFY(QMetaObject::invokeMethod(&first, "setConstantsDockVisible", Qt::DirectConnection,
                                         Q_ARG(bool, true), Q_ARG(bool, false)));
        MainWindow second(false);
        QVERIFY(QMetaObject::invokeMethod(&second, "setConstantsDockVisible", Qt::DirectConnection,
                                         Q_ARG(bool, false), Q_ARG(bool, false)));
        QVERIFY(QMetaObject::invokeMethod(&second, "setFunctionsDockVisible", Qt::DirectConnection,
                                         Q_ARG(bool, true), Q_ARG(bool, false)));
        first.persistSessionAndSettingsForShutdown();
    }
    MainWindow restored;
    const auto cleanup = qScopeGuard([&]() {
        for (MainWindow* window : topLevelMainWindows()) {
            if (window != &restored)
                delete window;
        }
    });
    restored.show();
    QTRY_COMPARE(topLevelMainWindows().size(), 2);
    for (MainWindow* window : topLevelMainWindows()) {
        QCOMPARE(window->findChildren<QDockWidget*>().size(), 1);
        const QString name = window->findChild<QDockWidget*>()->objectName();
        QCOMPARE(name, window == &restored ? QStringLiteral("ConstantsDock") : QStringLiteral("FunctionsDock"));
    }
}

void TestDockLayoutUi::opening_docks_keeps_floating_panels_independent()
{
    MainWindowStateGuard guard;
    guard.settings->sessionLayoutJson.clear();
    guard.settings->windowState.clear();
    guard.settings->formulaBookDockVisible = false;
    guard.settings->constantsDockVisible = false;
    guard.settings->functionsDockVisible = false;
    guard.settings->historyDockVisible = false;
    guard.settings->variablesDockVisible = false;
    guard.settings->userFunctionsDockVisible = false;
    guard.settings->userUnitsDockVisible = false;
    guard.settings->bitfieldVisible = false;
    MainWindow window(false);
    window.resize(960, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(QMetaObject::invokeMethod(&window, "setConstantsDockVisible", Qt::DirectConnection,
                                     Q_ARG(bool, true), Q_ARG(bool, false)));
    QDockWidget* constants = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constants);
    constants->setFloating(true);
    constants->move(180, 140);
    constants->resize(480, 360);
    QCoreApplication::processEvents();
    const QRect floatingGeometry = constants->geometry();

    QVERIFY(QMetaObject::invokeMethod(&window, "setFunctionsDockVisible", Qt::DirectConnection,
                                     Q_ARG(bool, true), Q_ARG(bool, false)));
    QDockWidget* functions = window.findChild<QDockWidget*>(QStringLiteral("FunctionsDock"));
    QVERIFY(functions);
    QTRY_VERIFY(functions->isVisible());
    QVERIFY(!functions->isFloating());
    QVERIFY(constants->isFloating());
    QCOMPARE(constants->parentWidget(), &window);
    QTRY_VERIFY(constants->isVisible());
    QCOMPARE(constants->geometry(), floatingGeometry);

    // New docked panels should still join the existing docked tab group.
    QVERIFY(QMetaObject::invokeMethod(&window, "setVariablesDockVisible", Qt::DirectConnection,
                                     Q_ARG(bool, true), Q_ARG(bool, false)));
    QDockWidget* variables = window.findChild<QDockWidget*>(QStringLiteral("VariablesDock"));
    QVERIFY(variables);
    QVERIFY(window.tabifiedDockWidgets(functions).contains(variables));
    QVERIFY(!window.tabifiedDockWidgets(functions).contains(constants));
    QCOMPARE(constants->parentWidget(), &window);
    QTRY_VERIFY(constants->isVisible());
    QCOMPARE(constants->geometry(), floatingGeometry);
}

void TestDockLayoutUi::floating_docks_survive_repeated_quit_data()
{
    QTest::addColumn<bool>("mixedLayout");
    QTest::addColumn<bool>("workspaceLayout");
    QTest::newRow("all-floating-workspace") << false << true;
    QTest::newRow("mixed-workspace") << true << true;
    QTest::newRow("all-floating-settings") << false << false;
    QTest::newRow("mixed-settings") << true << false;
}

void TestDockLayoutUi::floating_docks_survive_repeated_quit()
{
    QFETCH(bool, mixedLayout);
    QFETCH(bool, workspaceLayout);
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    const QVariant previousShutdown = qApp->property("speedcrunchShutdownInProgress");
    const auto restoreShutdown = qScopeGuard([&]() {
        qApp->setProperty("speedcrunchShutdownInProgress", previousShutdown);
    });

    class CloseFloatingDocksFirst : public QObject {
    public:
        bool receivedQuit = false;
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (watched == qApp && event->type() == QEvent::Quit) {
                receivedQuit = true;
                // Qt can close floating windows before the main window. Force
                // that order so the regression does not depend on widget order.
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    if (auto* dock = qobject_cast<QDockWidget*>(widget))
                        dock->close();
                }
                // Close the windows while keeping the test app's event loop usable.
                QApplication::closeAllWindows();
                return true;
            }
            return false;
        }
    } closeDocksFirst;
    // Application filters run in reverse installation order. MainWindow must
    // handle Quit before this filter starts closing the floating docks.
    qApp->installEventFilter(&closeDocksFirst);

    const QStringList names {QStringLiteral("BookDock"), QStringLiteral("ConstantsDock"),
        QStringLiteral("FunctionsDock"), QStringLiteral("UnitsDock"), QStringLiteral("HistoryDock"),
        QStringLiteral("VariablesDock"), QStringLiteral("UserFunctionsDock"),
        QStringLiteral("UserUnitsDock"), QStringLiteral("BitfieldDock")};
    const QList<QByteArray> setters {"setFormulaBookDockVisible", "setConstantsDockVisible",
        "setFunctionsDockVisible", "setUnitsDockVisible", "setHistoryDockVisible", "setVariablesDockVisible",
        "setUserFunctionsDockVisible", "setUserUnitsDockVisible", "setBitfieldVisible"};
    QStringList expectedOpen = names;
    if (mixedLayout)
        expectedOpen.removeAll(QStringLiteral("UserUnitsDock"));
    QHash<QString, QRect> floatingGeometry;

    // Save and relaunch twice, then check the third launch as well.
    for (int launch = 0; launch < 3; ++launch) {
        if (launch != 0) {
            guard.settings->load();
            if (!workspaceLayout)
                guard.settings->sessionLayoutJson.clear();
        }
        MainWindow window(launch != 0);
        window.resize(960, 640);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        if (launch == 0) {
            for (int i = 0; i < names.size(); ++i) {
                const bool opened = setters.at(i) == "setBitfieldVisible"
                    ? QMetaObject::invokeMethod(&window, setters.at(i).constData(), Qt::DirectConnection,
                                                Q_ARG(bool, true))
                    : QMetaObject::invokeMethod(&window, setters.at(i).constData(), Qt::DirectConnection,
                                                Q_ARG(bool, true), Q_ARG(bool, false));
                QVERIFY(opened);
                QDockWidget* dock = window.findChild<QDockWidget*>(names.at(i));
                QVERIFY(dock);
                dock->setFloating(!mixedLayout || i >= 2);
                if (dock->isFloating()) {
                    // Keep every dock inside the 800x600 offscreen screen.
                    dock->move(160 + i * 15, 120 + i * 12);
                    dock->resize(480, 360);
                }
                if (!expectedOpen.contains(names.at(i)))
                    dock->close();
            }
            QCoreApplication::processEvents();
            for (QDockWidget* dock : window.findChildren<QDockWidget*>()) {
                if (dock->isFloating())
                    floatingGeometry.insert(dock->objectName(), dock->geometry());
            }
        } else {
            QTRY_COMPARE(window.findChildren<QDockWidget*>().size(), expectedOpen.size());
        }
        for (int i = 0; i < names.size(); ++i) {
            QDockWidget* dock = window.findChild<QDockWidget*>(names.at(i));
            if (!expectedOpen.contains(names.at(i))) {
                QVERIFY(!dock || dock->isHidden());
                continue;
            }
            QVERIFY(dock);
            QTRY_VERIFY(dock->isVisible());
            QCOMPARE(dock->isFloating(), !mixedLayout || i >= 2);
            if (dock->isFloating())
                QTRY_COMPARE(dock->geometry(), floatingGeometry.value(names.at(i)));
        }

        // main.cpp also publishes this flag before QApplication handles Quit.
        qApp->setProperty("speedcrunchShutdownInProgress", true);
        closeDocksFirst.receivedQuit = false;
        QEvent quit(QEvent::Quit);
        QCoreApplication::sendEvent(qApp, &quit);
        QVERIFY(closeDocksFirst.receivedQuit);
        QVERIFY(!window.isVisible());
        const QJsonArray windows = QJsonDocument::fromJson(guard.settings->sessionLayoutJson.toUtf8())
                                       .object().value(QStringLiteral("windows")).toArray();
        QCOMPARE(windows.size(), 1);
        QCOMPARE(windows.first().toObject().value(QStringLiteral("openDocks")).toArray(),
                 QJsonArray::fromStringList(expectedOpen));
        qApp->setProperty("speedcrunchShutdownInProgress", previousShutdown);
    }
}

void TestDockLayoutUi::dock_sizes_survive_relaunch_data()
{
    QTest::addColumn<bool>("maximized");
    QTest::addColumn<bool>("workspaceLayout");
    QTest::newRow("maximized-workspace") << true << true;
    QTest::newRow("maximized-settings") << true << false;
    QTest::newRow("normal-workspace") << false << true;
    QTest::newRow("normal-settings") << false << false;
}

void TestDockLayoutUi::dock_sizes_survive_relaunch()
{
    QFETCH(bool, maximized);
    QFETCH(bool, workspaceLayout);
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    guard.settings->windowPositionSave = true;
    guard.settings->keypadVisible = false;
    guard.settings->keypadMode = Settings::KeypadModeDisabled;
    QHash<QString, QSize> expectedSizes;
    QSize expectedWindowSize;
    const QStringList names {QStringLiteral("ConstantsDock"), QStringLiteral("FunctionsDock"),
        QStringLiteral("VariablesDock")};

    for (int launch = 0; launch < 3; ++launch) {
        if (launch != 0) {
            guard.settings->load();
            if (!workspaceLayout)
                guard.settings->sessionLayoutJson.clear();
        }
        MainWindow window(launch != 0);
        if (launch == 0) {
            window.resize(720, 540);
            for (const char* setter : {"setConstantsDockVisible", "setFunctionsDockVisible", "setVariablesDockVisible"})
                QVERIFY(QMetaObject::invokeMethod(&window, setter, Qt::DirectConnection,
                                                 Q_ARG(bool, true), Q_ARG(bool, false)));
            auto* constants = window.findChild<QDockWidget*>(names.at(0));
            auto* functions = window.findChild<QDockWidget*>(names.at(1));
            auto* variables = window.findChild<QDockWidget*>(names.at(2));
            QVERIFY(constants && functions && variables);
            window.addDockWidget(Qt::LeftDockWidgetArea, constants);
            window.splitDockWidget(constants, functions, Qt::Vertical);
            window.addDockWidget(Qt::RightDockWidgetArea, variables);
            if (maximized)
                window.showMaximized();
        }
        window.showWhenReady();
        QTRY_VERIFY(window.isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(window.isMaximized(), maximized);
        QTRY_COMPARE(window.findChildren<QDockWidget*>().size(), names.size());
        if (launch == 0) {
            auto* constants = window.findChild<QDockWidget*>(names.at(0));
            auto* functions = window.findChild<QDockWidget*>(names.at(1));
            auto* variables = window.findChild<QDockWidget*>(names.at(2));
            window.resizeDocks({constants, variables},
                               {window.width() / 4, window.width() / 3}, Qt::Horizontal);
            window.resizeDocks({constants, functions}, {200, 300}, Qt::Vertical);
            QTest::qWait(200);
            expectedWindowSize = window.size();
            for (const QString& name : names)
                expectedSizes.insert(name, window.findChild<QDockWidget*>(name)->size());
        } else {
            if (!maximized)
                QTRY_COMPARE(window.size(), expectedWindowSize);
            for (const QString& name : names) {
                auto* dock = window.findChild<QDockWidget*>(name);
                QVERIFY(dock);
                QVERIFY(!dock->isHidden());
                QTRY_COMPARE(dock->width(), expectedSizes.value(name).width());
                // Qt can round a stacked dock's height by one pixel when
                // fitting the saved vertical split to the window again.
                QTRY_VERIFY(qAbs(dock->height() - expectedSizes.value(name).height()) <= 1);
                QCOMPARE(window.dockWidgetArea(dock), name == names.last()
                    ? Qt::RightDockWidgetArea : Qt::LeftDockWidgetArea);
            }
        }
        window.persistSessionAndSettingsForShutdown();
    }
}

void TestDockLayoutUi::fullscreen_dock_widths_survive_relaunch_data()
{
    QTest::addColumn<bool>("workspaceLayout");
    QTest::newRow("workspace") << true;
    QTest::newRow("settings") << false;
}

void TestDockLayoutUi::fullscreen_dock_widths_survive_relaunch()
{
    if (QGuiApplication::platformName() == QLatin1String("offscreen")
        || QGuiApplication::platformName() == QLatin1String("minimal"))
        QSKIP("Requires a native window manager to restore fullscreen geometry.", );
    QFETCH(bool, workspaceLayout);
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    guard.settings->windowPositionSave = true;
    guard.settings->keypadVisible = false;
    guard.settings->keypadMode = Settings::KeypadModeDisabled;
    const QStringList names {QStringLiteral("ConstantsDock"), QStringLiteral("VariablesDock")};
    QHash<QString, int> widths;
    QSize fullscreenSize;

    for (int launch = 0; launch < 3; ++launch) {
        if (launch != 0) {
            guard.settings->load();
            if (!workspaceLayout)
                guard.settings->sessionLayoutJson.clear();
        }
        MainWindow window(launch != 0);
        ReportedWindowState reported(&window);
        const auto exitFullscreen = [&]() {
            if (!window.isFullScreen() && !reported.isFullScreen())
                return true;
            const bool exited = reported.changeFullScreen(false, [&]() { window.showNormal(); });
            if (exited)
                QTest::qWait(1500);
            return exited && !window.isFullScreen();
        };
        auto cleanupFullscreen = qScopeGuard([&]() { exitFullscreen(); });
        if (launch == 0) {
            window.resize(720, 540);
            for (const char* setter : {"setConstantsDockVisible", "setVariablesDockVisible"})
                QVERIFY(QMetaObject::invokeMethod(&window, setter, Qt::DirectConnection,
                                                 Q_ARG(bool, true), Q_ARG(bool, false)));
            window.addDockWidget(Qt::LeftDockWidgetArea, window.findChild<QDockWidget*>(names.first()));
            window.addDockWidget(Qt::RightDockWidgetArea, window.findChild<QDockWidget*>(names.last()));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
#ifdef Q_OS_MACOS
            // Cocoa needs the application in front before entering fullscreen.
            // activateWindow() alone only makes the native window key.
            window.raise();
            if (!QTest::qWaitFor([]() {
                    return QGuiApplication::applicationState() == Qt::ApplicationActive;
                }, 5000))
                QSKIP("macOS declines test application activation in this desktop session.", );
#endif
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
        }
        const bool entered = reported.changeFullScreen(true, [&]() {
            if (launch == 0)
                window.showFullScreen();
            else
                window.showWhenReady();
        });
        QVERIFY2(entered, qPrintable(QStringLiteral("Launch %1 did not enter fullscreen "
                                      "(active=%2, visible=%3, requested=%4)")
                .arg(launch + 1).arg(window.isActiveWindow())
                .arg(window.isVisible()).arg(window.isFullScreen())));
        QTRY_VERIFY(window.isVisible() && window.isFullScreen());
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(500);
        QTRY_COMPARE(window.findChildren<QDockWidget*>().size(), names.size());
        if (launch == 0) {
            window.resizeDocks({window.findChild<QDockWidget*>(names.first()),
                                window.findChild<QDockWidget*>(names.last())},
                               {window.width() / 4, window.width() / 3}, Qt::Horizontal);
            QTest::qWait(200);
            fullscreenSize = window.size();
            for (const QString& name : names)
                widths.insert(name, window.findChild<QDockWidget*>(name)->width());
        } else {
            QTRY_COMPARE(window.size(), fullscreenSize);
            for (const QString& name : names) {
                auto* dock = window.findChild<QDockWidget*>(name);
                QVERIFY(dock && !dock->isHidden() && !dock->isFloating());
                QTRY_COMPARE(dock->width(), widths.value(name));
            }
        }
        window.persistSessionAndSettingsForShutdown();
        // Leave the fullscreen Space before destroying the test window, so
        // its exit animation does not affect the next launch or test case.
        cleanupFullscreen.dismiss();
        QVERIFY2(exitFullscreen(), "Native fullscreen exit does not complete");
    }
}

void TestDockLayoutUi::dock_list_selected_row_keeps_primary_fill_while_hovered()
{
    const QColor background(QStringLiteral("#202124"));
    const QColor foreground(QStringLiteral("#d6d8dc"));
    const QColor hoverBackground(QStringLiteral("#4a5568"));
    const QColor hoverForeground(QStringLiteral("#f8fafc"));
    const QColor primaryBackground(QStringLiteral("#2f80ed"));
    const QColor primaryForeground(QStringLiteral("#ffffff"));
    const QColor inactiveBackground(QStringLiteral("#3b4252"));
    const QColor inactiveForeground(QStringLiteral("#eceff4"));

    QTreeWidget table;
    table.setColumnCount(3);
    table.setRootIsDecorated(false);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.header()->hide();
    DockListStyle::apply(&table);
    table.setProperty("dockListHoverBackground", hoverBackground);
    table.setProperty("dockListHoverForeground", hoverForeground);
    table.setProperty("dockListActiveSelectionBackground", primaryBackground);
    table.setProperty("dockListActiveSelectionForeground", primaryForeground);
    table.setProperty("dockListInactiveSelectionBackground", inactiveBackground);
    table.setProperty("dockListInactiveSelectionForeground", inactiveForeground);
    table.setStyleSheet(QStringLiteral(
        "QAbstractItemView { background-color: %1; color: %2; border: 0; }")
                            .arg(background.name(),
                                 foreground.name()));

    auto* item = new QTreeWidgetItem(&table, QStringList{
        QStringLiteral("x"),
        QStringLiteral("42"),
        QStringLiteral("m")
    });
    table.setColumnWidth(0, 70);
    table.setColumnWidth(1, 70);
    table.setColumnWidth(2, 70);
    table.resize(260, 80);
    table.show();
    QVERIFY(QTest::qWaitForWindowExposed(&table));
    table.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&table));

    const QModelIndex index = table.indexFromItem(item, 0);
    const QModelIndex secondColumnIndex = table.indexFromItem(item, 1);
    const QRect itemRect = table.visualRect(index);
    const QRect secondColumnRect = table.visualRect(secondColumnIndex);
    QVERIFY(itemRect.isValid());
    QVERIFY(secondColumnRect.isValid());
    const auto imagePoint = [&table](QPoint point) {
        const qreal scale = table.viewport()->devicePixelRatioF();
        return QPoint(qRound(point.x() * scale), qRound(point.y() * scale));
    };
    const QPoint sampledFill = imagePoint(QPoint(itemRect.right() - 4, itemRect.center().y()));
    const QPoint sampledColumnBoundary = imagePoint(QPoint(secondColumnRect.left() + 1, itemRect.top() + 2));
    const QPoint sampledRoundedCorner = imagePoint(QPoint(itemRect.left() + 1, itemRect.top() + 1));

    QTest::mouseMove(table.windowHandle(), table.viewport()->mapTo(&table, itemRect.center()));
    QTRY_COMPARE(table.property("dockListHoveredRow").toInt(), 0);
    QImage hoveredImage = table.viewport()->grab().toImage();
    QVERIFY2(colorsAreClose(hoveredImage.pixelColor(sampledFill), hoverBackground, 24),
             qPrintable(QStringLiteral("hover sample is %1, expected %2")
                            .arg(hoveredImage.pixelColor(sampledFill).name(),
                                 hoverBackground.name())));
    QVERIFY2(colorsAreClose(hoveredImage.pixelColor(sampledColumnBoundary), hoverBackground, 32),
             qPrintable(QStringLiteral("column-boundary hover sample is %1, expected %2")
                            .arg(hoveredImage.pixelColor(sampledColumnBoundary).name(),
                                 hoverBackground.name())));
    QVERIFY(!colorsAreClose(hoveredImage.pixelColor(sampledRoundedCorner), hoverBackground, 8));
    QVERIFY(hoveredImage.pixelColor(sampledFill).name() != primaryBackground.name());

    table.setCurrentItem(item);
    item->setSelected(true);
    table.setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(table.hasFocus());
    QTRY_COMPARE(table.viewport()->grab().toImage().pixelColor(sampledFill).name(),
                 primaryBackground.name());
}

void TestDockLayoutUi::functions_dock_retranslates_domain_label_after_language_change()
{
    FunctionsWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QLabel* domainLabel = nullptr;
    for (QLabel* label : widget.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Domain")) {
            domainLabel = label;
            break;
        }
    }
    QVERIFY(domainLabel != nullptr);

    FunctionsTestTranslator translator;
    QCoreApplication::installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&widget, &languageChange);

    QCOMPARE(domainLabel->text(), QStringLiteral("Translated Domain"));

    QCoreApplication::removeTranslator(&translator);
}

void TestDockLayoutUi::functions_dock_finds_geometry_by_name()
{
    FunctionsWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.restoreState(QStringLiteral("Geometry"), QStringLiteral("Surface Area of a Cylinder"));
    QCOMPARE(widget.selectedDomain(), QStringLiteral("Geometry"));

    auto* list = widget.findChild<QTreeWidget*>();
    QVERIFY(list);
    QTRY_COMPARE(list->topLevelItemCount(), 1);
    QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("area_cylinder"));
}

void TestDockLayoutUi::side_docks_keep_full_height_data()
{
    QTest::addColumn<int>("restorePath");
    QTest::newRow("fresh-layout") << 0;
    QTest::newRow("saved-settings") << 1;
    QTest::newRow("session-layout") << 2;
    QTest::newRow("new-window") << 3;
}

void TestDockLayoutUi::side_docks_keep_full_height()
{
    QFETCH(int, restorePath);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = true;
    settings->historyDockVisible = true;
    settings->bitfieldVisible = true;

    MainWindow source(false);
    for (QDockWidget* dock : source.findChildren<QDockWidget*>())
        dock->hide();

    struct DockSpec {
        const char* name;
        Qt::DockWidgetArea area;
    };
    const DockSpec docks[] = {
        { "ConstantsDock", Qt::LeftDockWidgetArea },
        { "FunctionsDock", Qt::RightDockWidgetArea },
        { "HistoryDock", Qt::TopDockWidgetArea },
        { "BitfieldDock", Qt::BottomDockWidgetArea }
    };
    for (const DockSpec& spec : docks) {
        QDockWidget* dock = source.findChild<QDockWidget*>(QString::fromLatin1(spec.name));
        QVERIFY(dock != nullptr);
        source.removeDockWidget(dock);
        source.addDockWidget(spec.area, dock);
        dock->show();
    }
    source.resize(1500, 1000);
    source.show();
    QVERIFY(QTest::qWaitForWindowExposed(&source));

    QPointer<MainWindow> restored;
    const auto cleanup = qScopeGuard([&restored]() { delete restored.data(); });
    MainWindow* window = &source;
    if (restorePath != 0) {
        // Older layouts can give the top and bottom docks all four corners.
        source.setCorner(Qt::TopLeftCorner, Qt::TopDockWidgetArea);
        source.setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
        source.setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
        source.setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
        const QByteArray state = source.saveState(1);
        if (restorePath == 3) {
            const QList<MainWindow*> windowsBefore = topLevelMainWindows();
            QVERIFY(QMetaObject::invokeMethod(&source, "showNewSessionWindow", Qt::DirectConnection));
            QTRY_VERIFY(([&]() {
                for (MainWindow* candidate : topLevelMainWindows()) {
                    if (!windowsBefore.contains(candidate)) {
                        restored = candidate;
                        return true;
                    }
                }
                return false;
            }()));
        } else {
            if (restorePath == 1)
                settings->windowState = state;
            restored = new MainWindow(false);
            if (restorePath == 2) {
                QVERIFY(QMetaObject::invokeMethod(restored.data(), "restoreWindowLayoutState",
                                                  Qt::DirectConnection, Q_ARG(QByteArray, state)));
            }
        }
        window = restored.data();
        window->resize(1500, 1000);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    QCOMPARE(window->corner(Qt::TopLeftCorner), Qt::LeftDockWidgetArea);
    QCOMPARE(window->corner(Qt::BottomLeftCorner), Qt::LeftDockWidgetArea);
    QCOMPARE(window->corner(Qt::TopRightCorner), Qt::RightDockWidgetArea);
    QCOMPARE(window->corner(Qt::BottomRightCorner), Qt::RightDockWidgetArea);

    QList<QDockWidget*> attached;
    for (const DockSpec& spec : docks) {
        QDockWidget* dock = window->findChild<QDockWidget*>(QString::fromLatin1(spec.name));
        QVERIFY(dock != nullptr);
        QTRY_VERIFY(dock->isVisible());
        QVERIFY(!dock->isFloating());
        QCOMPARE(window->dockWidgetArea(dock), spec.area);
        attached.append(dock);
    }
    QVERIFY(window->findChild<Keypad*>() != nullptr);
    QDockWidget* left = attached.at(0);
    QDockWidget* right = attached.at(1);
    QDockWidget* top = attached.at(2);
    QDockWidget* bottom = attached.at(3);
    for (QDockWidget* side : { left, right }) {
        QTRY_COMPARE(side->geometry().top(), top->geometry().top());
        QTRY_COMPARE(side->geometry().bottom(), bottom->geometry().bottom());
    }
    for (QDockWidget* horizontal : { top, bottom }) {
        QTRY_VERIFY(horizontal->geometry().left() > left->geometry().right());
        QTRY_VERIFY(horizontal->geometry().right() < right->geometry().left());
        QTRY_COMPARE(horizontal->geometry().left(), window->centralWidget()->geometry().left());
        QTRY_COMPARE(horizontal->geometry().right(), window->centralWidget()->geometry().right());
    }
}

void TestDockLayoutUi::dock_search_rows_use_configured_vertical_padding_data()
{
    QTest::addColumn<QString>("dock");
    QTest::addColumn<int>("topPadding");
    for (const QString& dock : {QStringLiteral("constants"), QStringLiteral("functions")})
        QTest::newRow(qPrintable(dock)) << dock << UiConfig::DockSearchRowVerticalPadding;
    for (const QString& dock : {QStringLiteral("user-variables"),
                               QStringLiteral("user-functions"),
                               QStringLiteral("user-units")})
        QTest::newRow(qPrintable(dock)) << dock << 6;
}

void TestDockLayoutUi::dock_search_rows_use_configured_vertical_padding()
{
    QFETCH(QString, dock);
    QFETCH(int, topPadding);

    QWidget owner;
    QWidget* widget = nullptr;
    if (dock == QStringLiteral("constants"))
        widget = new ConstantsWidget(&owner);
    else if (dock == QStringLiteral("functions"))
        widget = new FunctionsWidget(&owner);
    else if (dock == QStringLiteral("user-variables"))
        widget = new VariableListWidget(&owner);
    else if (dock == QStringLiteral("user-functions"))
        widget = new UserFunctionListWidget(&owner);
    else if (dock == QStringLiteral("user-units"))
        widget = new UserUnitListWidget(&owner);
    QVERIFY(widget);

    widget->resize(420, 360);
    widget->show();
    QLineEdit* search = widget->findChild<QLineEdit*>();
    QTreeWidget* list = widget->findChild<QTreeWidget*>();
    QVERIFY(search);
    QVERIFY(list);
    QWidget* row = search->parentWidget();
    QVERIFY(row->layout());
    widget->layout()->activate();
    row->layout()->activate();

    QCOMPARE(row->layout()->contentsMargins(),
             QMargins(8, topPadding, 8, UiConfig::DockSearchRowVerticalPadding));
    const int searchTop = search->mapTo(widget, QPoint()).y();
    const int rowTop = row->mapTo(widget, QPoint()).y();
    QCOMPARE(searchTop - rowTop, topPadding);
    QCOMPARE(list->y() - (searchTop + search->height()),
             UiConfig::DockSearchRowVerticalPadding);
}

void TestDockLayoutUi::themed_dock_lists_use_compact_spacing_data()
{
    QTest::addColumn<QString>("dockName");
    QTest::addColumn<bool>("light");
    for (bool light : {false, true}) {
        for (const QString& dock : {QStringLiteral("ConstantsDock"), QStringLiteral("FunctionsDock"),
                                    QStringLiteral("VariablesDock"), QStringLiteral("UserFunctionsDock"),
                                    QStringLiteral("UserUnitsDock"), QStringLiteral("HistoryDock")}) {
            const QString name = (light ? QStringLiteral("light-") : QStringLiteral("dark-")) + dock;
            QTest::newRow(qPrintable(name)) << dock << light;
        }
    }
}

void TestDockLayoutUi::themed_dock_lists_use_compact_spacing()
{
    QFETCH(QString, dockName);
    QFETCH(bool, light);
    MainWindowStateGuard guard;
    guard.settings->dockAppearance = Settings::DockAppearanceSystem;
    guard.settings->constantsDockVisible = true;
    guard.settings->functionsDockVisible = true;
    guard.settings->variablesDockVisible = true;
    guard.settings->userFunctionsDockVisible = true;
    guard.settings->userUnitsDockVisible = true;
    guard.settings->historyDockVisible = true;
    guard.settings->windowState.clear();
    guard.settings->sessionLayoutJson.clear();
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), QString(light ? "#e5eee8" : "#27314a")}});
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QMenu* appearance = window.findChild<QMenu*>(QStringLiteral("DockAppearanceMenu"));
    QVERIFY(dock);
    QVERIFY(appearance);
    dock->setFloating(true);
    dock->resize(500, 400);
    dock->show();
    QAbstractItemView* list = nullptr;
    for (QAbstractItemView* view : dock->findChildren<QAbstractItemView*>()) {
        if (!qobject_cast<QHeaderView*>(view) && !qobject_cast<QComboBox*>(view->parentWidget())) {
            if (qobject_cast<QTreeWidget*>(view) || qobject_cast<QListWidget*>(view)) {
                list = view;
                break;
            }
        }
    }
    QVERIFY(list);
    QCoreApplication::processEvents();
    const auto firstRow = [list]() {
        if (list->model()->rowCount() == 0) {
            if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(list))
                new QTreeWidgetItem(tree, QStringList{QStringLiteral("Sample"), QStringLiteral("123")});
            else
                qobject_cast<QListWidget*>(list)->addItem(QStringLiteral("Sample"));
        }
        list->doItemsLayout();
        return list->model()->index(0, 0);
    };
    const QModelIndex first = firstRow();
    QVERIFY(first.isValid());
    const int nativeRowHeight = list->visualRect(first).height();
    const int nativeInset = list->viewport()->x();
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(list);
    const int nativeHeaderHeight = tree ? tree->header()->height() : 0;
    for (int cycle = 0; cycle < 2; ++cycle) {
        appearance->actions().at(1)->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(list->contentsMargins(), QMargins());
        QVERIFY(list->viewport()->x() <= nativeInset);
        QCOMPARE(list->visualRect(firstRow()).height(), nativeRowHeight);
        if (tree) {
            QCOMPARE(tree->header()->height(), nativeHeaderHeight);
        }
        appearance->actions().at(0)->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(list->viewport()->x(), nativeInset);
        if (tree)
            QCOMPARE(tree->header()->height(), nativeHeaderHeight);
    }
}

#ifdef Q_OS_WIN
void TestDockLayoutUi::dock_list_rows_match_tree_rows_data()
{
    QTest::addColumn<bool>("windows11");
    QTest::addColumn<bool>("styleSheet");
    QTest::newRow("padded-style") << false << false;
    QTest::newRow("padded-style-with-style-sheet") << false << true;
    if (QStyleFactory::keys().contains(QStringLiteral("windows11"), Qt::CaseInsensitive)) {
        QTest::newRow("windows11") << true << false;
        QTest::newRow("windows11-with-style-sheet") << true << true;
    }
}

void TestDockLayoutUi::dock_list_rows_match_tree_rows()
{
    QFETCH(bool, windows11);
    QFETCH(bool, styleSheet);
    QScopedPointer<QStyle> style(windows11 ? QStyleFactory::create(QStringLiteral("windows11"))
                                           : new ListRowPaddingTestStyle);
    QVERIFY(style);
    const QString sheet = styleSheet ? QStringLiteral("QAbstractItemView { border: 0; }") : QString();

    QTreeWidget tree;
    tree.setStyle(style.data());
    tree.setRootIsDecorated(false);
    tree.header()->hide();
    tree.setStyleSheet(sheet);
    DockListStyle::apply(&tree);
    QTreeWidgetItem* treeItem = new QTreeWidgetItem(&tree, QStringList{QStringLiteral("Sample")});

    QListWidget list;
    list.setStyle(style.data());
    list.setStyleSheet(sheet);
    DockListStyle::apply(&list);
    list.addItem(QStringLiteral("Sample"));

    tree.resize(200, 100);
    list.resize(200, 100);
    tree.show();
    list.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tree));
    QVERIFY(QTest::qWaitForWindowExposed(&list));
    tree.doItemsLayout();
    list.doItemsLayout();

    const QModelIndex listIndex = list.model()->index(0, 0);
    QCOMPARE(list.visualRect(listIndex).height(), tree.visualItemRect(treeItem).height());
    const int textStart = firstPaintedColumn(&tree);
    QVERIFY(textStart >= 0);
    QCOMPARE(firstPaintedColumn(&list), textStart);
}
#endif

void TestDockLayoutUi::themed_dock_controls_have_equal_visible_gaps_data()
{
    QTest::addColumn<QString>("dockName");
    QTest::addColumn<bool>("wide");
    QTest::addColumn<bool>("light");
    QTest::addColumn<bool>("subdomains");
    for (bool light : {false, true}) {
        const QByteArray theme = light ? "light-" : "dark-";
        for (const QString& dock : {QStringLiteral("ConstantsDock"),
                                   QStringLiteral("FunctionsDock"),
                                   QStringLiteral("VariablesDock"),
                                   QStringLiteral("UserFunctionsDock"),
                                   QStringLiteral("UserUnitsDock")}) {
            const QByteArray name = theme + dock.toUtf8();
            QTest::newRow(name.constData()) << dock << false << light << true;
        }
        const QByteArray wideName = theme + "constants-wide";
        QTest::newRow(wideName.constData())
            << QStringLiteral("ConstantsDock") << true << light << true;
        for (bool wide : {false, true}) {
            const QByteArray name = theme + (wide ? "constants-single-wide" : "constants-single");
            QTest::newRow(name.constData())
                << QStringLiteral("ConstantsDock") << wide << light << false;
        }
    }
}

void TestDockLayoutUi::themed_dock_controls_have_equal_visible_gaps()
{
    QFETCH(QString, dockName);
    QFETCH(bool, wide);
    QFETCH(bool, light);
    QFETCH(bool, subdomains);

    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = dockName == QStringLiteral("ConstantsDock");
    settings->functionsDockVisible = dockName == QStringLiteral("FunctionsDock");
    settings->variablesDockVisible = dockName == QStringLiteral("VariablesDock");
    settings->userFunctionsDockVisible = dockName == QStringLiteral("UserFunctionsDock");
    settings->userUnitsDockVisible = dockName == QStringLiteral("UserUnitsDock");
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadVisible = false;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject {
        {QStringLiteral("background"), light ? QStringLiteral("#f0f6ef")
                                             : QStringLiteral("#1f3229")}
    });

    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QVERIFY(dock);
    dock->setFloating(true);
    dock->resize(wide ? 800 : 320, 480);

    QWidget* content = dock->widget();
    if (ConstantsWidget* constants = qobject_cast<ConstantsWidget*>(content)) {
        QComboBox* domain = constants->findChild<QComboBox*>();
        QVERIFY(domain);
        constants->restoreState(domain->itemText(subdomains ? 0 : 1), QString(), QString());
    }
    QCoreApplication::processEvents();
    QTest::qWait(30);
    QLineEdit* search = content->findChild<QLineEdit*>();
    QTreeWidget* table = content->findChild<QTreeWidget*>();
    QVERIFY(search);
    QVERIFY(table);
    QVERIFY(table->header()->isVisible());
    const auto top = [content](QWidget* widget) {
        return widget->mapTo(content, QPoint()).y();
    };
    const auto bottom = [&top](QWidget* widget) {
        return top(widget) + widget->height();
    };

    // Measure the painted header, including any padding inside the table.
    QCOMPARE(top(table->header()->viewport()) - bottom(search),
             UiConfig::DockSearchRowVerticalPadding);
    QList<QComboBox*> combos;
    for (QComboBox* combo : content->findChildren<QComboBox*>()) {
        if (combo->isVisible())
            combos.append(combo);
    }
    if (!combos.isEmpty()) {
        if (dockName == QStringLiteral("ConstantsDock"))
            QCOMPARE(combos.size(), subdomains ? 2 : 1);
        QComboBox* lowest = combos.first();
        for (QComboBox* combo : combos) {
            if (bottom(combo) > bottom(lowest))
                lowest = combo;
        }
        QCOMPARE(top(search) - bottom(lowest), UiConfig::DockSearchRowVerticalPadding);
        if (combos.size() == 2) {
            if (wide) {
                QCOMPARE(top(combos.first()), top(combos.last()));
            } else {
                QComboBox* upper = top(combos.first()) < top(combos.last())
                    ? combos.first() : combos.last();
                QComboBox* lower = upper == combos.first() ? combos.last() : combos.first();
                QCOMPARE(top(lower) - bottom(upper), UiConfig::DockSearchRowVerticalPadding);
            }
        }
    }
}

void TestDockLayoutUi::dock_first_control_matches_shared_vertical_spacing_data()
{
    QTest::addColumn<bool>("light");
    QTest::newRow("dark") << false;
    QTest::newRow("light") << true;
}

void TestDockLayoutUi::dock_first_control_matches_shared_vertical_spacing()
{
    QFETCH(bool, light);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = true;
    settings->variablesDockVisible = true;
    settings->userFunctionsDockVisible = true;
    settings->userUnitsDockVisible = true;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadVisible = false;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject {
        {QStringLiteral("background"), light ? QStringLiteral("#f0f6ef")
                                             : QStringLiteral("#1f3229")}
    });

    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    for (const QString& name : {QStringLiteral("ConstantsDock"),
                               QStringLiteral("FunctionsDock"),
                               QStringLiteral("VariablesDock"),
                               QStringLiteral("UserFunctionsDock"),
                               QStringLiteral("UserUnitsDock")}) {
        QDockWidget* dock = window.findChild<QDockWidget*>(name);
        QVERIFY(dock);
        dock->setFloating(true);
        dock->resize(320, 480);
        dock->show();
        QCoreApplication::processEvents();
        QWidget* content = dock->widget();
        QWidget* first = content->findChild<QLineEdit*>();
        QVERIFY(first);
        for (QComboBox* combo : content->findChildren<QComboBox*>()) {
            if (combo->isVisible()
                && combo->mapTo(content, QPoint()).y() < first->mapTo(content, QPoint()).y())
                first = combo;
        }
        QTRY_COMPARE(first->mapTo(content, QPoint()).y(),
                     UiConfig::DockSearchRowVerticalPadding);
    }
}

void TestDockLayoutUi::formula_book_links_follow_dock_appearance_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("systemAtStartup");
    QTest::newRow("dark-system-startup") << true << true;
    QTest::newRow("light-system-startup") << false << true;
    QTest::newRow("dark-switch-from-theme") << true << false;
    QTest::newRow("light-switch-from-theme") << false << false;
}

void TestDockLayoutUi::formula_book_links_follow_dock_appearance()
{
    QFETCH(bool, dark);
    QFETCH(bool, systemAtStartup);
    MainWindowStateGuard guard;
    const QPalette original = QApplication::palette();
    const auto restorePalette = qScopeGuard([original]() { QApplication::setPalette(original); });
    QPalette native = original;
    native.setColor(QPalette::Base, QColor(dark ? "#202020" : "#fafafa"));
    native.setColor(QPalette::Text, QColor(dark ? "#eeeeee" : "#202020"));
    // Keep the system link color even when a contrast adjustment would change it.
    native.setColor(QPalette::Link, QColor(dark ? "#343c4b" : "#c1c7d0"));
    QApplication::setPalette(native);
    guard.settings->dockAppearance = systemAtStartup ? Settings::DockAppearanceSystem : Settings::DockAppearanceSpeedCrunch;
    guard.settings->formulaBookDockVisible = true;
    guard.settings->windowState.clear();
    guard.settings->sessionLayoutJson.clear();
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), QStringLiteral("#27314a")}});
    MainWindow first(false);
    MainWindow second(false);
    first.show();
    second.show();
    QVERIFY(QTest::qWaitForWindowExposed(&first));
    QVERIFY(QTest::qWaitForWindowExposed(&second));
    QMenu* appearance = first.findChild<QMenu*>(QStringLiteral("DockAppearanceMenu"));
    QVERIFY(appearance);
    const auto verifyLinks = [&](BookDock* book, bool system, const QColor& nativeLink) {
        QTextBrowser* browser = book->findChild<QTextBrowser*>();
        QVERIFY(browser);
        const QColor background = browser->palette().color(QPalette::Base);
        const QColor formulaColor = system ? nativeLink
            : aaForegroundForBackground(background, browser->palette().color(QPalette::Link));
        const QColor navigationColor = system ? nativeLink
            : generateSecondaryLinkFromBackground(background, formulaColor);
        int navigationLinks = 0;
        int formulaLinks = 0;
        for (QTextBlock block = browser->document()->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextCharFormat format = it.fragment().charFormat();
                if (!format.isAnchor())
                    continue;
                const QString href = format.anchorHref();
                const bool formula = href.startsWith(QStringLiteral("formula:"))
                    || href.startsWith(QStringLiteral("function:"));
                QCOMPARE(format.foreground().color().rgba(), (formula ? formulaColor : navigationColor).rgba());
                if (formula)
                    ++formulaLinks;
                else
                    ++navigationLinks;
            }
        }
        QVERIFY(navigationLinks > 0);
        QCOMPARE(formulaLinks > 0, book->currentPage() != QStringLiteral("index"));
    };
    for (bool system : {systemAtStartup, !systemAtStartup, systemAtStartup}) {
        appearance->actions().at(system ? 0 : 1)->trigger();
        QCoreApplication::processEvents();
        for (MainWindow* window : {&first, &second}) {
            BookDock* book = window->findChild<BookDock*>(QStringLiteral("BookDock"));
            QVERIFY(book);
            for (const QString& page : {QStringLiteral("index"), QStringLiteral("electronics/ohmslaw"),
                                        QStringLiteral("geometry/circle")}) {
                book->openPage(QUrl(page));
                verifyLinks(book, system, native.color(QPalette::Link));
                if (QTest::currentTestFailed())
                    return;
            }
        }
    }
    appearance->actions().at(0)->trigger();
    native.setColor(QPalette::Link, QColor(dark ? "#b8a0eb" : "#6321a0"));
    QApplication::setPalette(native);
    QCoreApplication::processEvents();
    for (MainWindow* window : {&first, &second}) {
        BookDock* book = window->findChild<BookDock*>(QStringLiteral("BookDock"));
        QVERIFY(book);
        QTextBrowser* browser = book->findChild<QTextBrowser*>();
        QVERIFY(browser);
        QTRY_COMPARE(browser->palette().color(QPalette::Link).rgba(), native.color(QPalette::Link).rgba());
        QCOMPARE(book->currentPage(), QStringLiteral("geometry/circle"));
        verifyLinks(book, true, native.color(QPalette::Link));
        if (QTest::currentTestFailed())
            return;
        book->openPage(QUrl(QStringLiteral("index")));
        verifyLinks(book, true, native.color(QPalette::Link));
        if (QTest::currentTestFailed())
            return;
    }
}

void TestDockLayoutUi::formula_book_text_scales_with_zoom_data()
{
    QTest::addColumn<QString>("page");
    QTest::addColumn<QString>("formula");
    QTest::addColumn<QString>("caption");
    QTest::addColumn<QString>("variable");
    QTest::addColumn<QString>("unit");

    QTest::newRow("geometry")
        << QStringLiteral("geometry/circle") << QStringLiteral("A =")
        << QStringLiteral("radius") << QStringLiteral("r") << QString();
    QTest::newRow("electronics")
        << QStringLiteral("electronics/ohmslaw") << QStringLiteral("R =")
        << QStringLiteral("resistance") << QStringLiteral("R") << QStringLiteral("Ω");
    QTest::newRow("compound-unit")
        << QStringLiteral("rf/propagation") << QStringLiteral("= 3e8")
        << QStringLiteral("dielectric constant") << QStringLiteral("e") << QStringLiteral("m·s");
}

void TestDockLayoutUi::formula_book_text_scales_with_zoom()
{
    QFETCH(QString, page);
    QFETCH(QString, formula);
    QFETCH(QString, caption);
    QFETCH(QString, variable);
    QFETCH(QString, unit);

    BookDock dock;
    dock.resize(800, 600);
    dock.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dock));
    dock.openPage(QUrl(page));
    QTextBrowser* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser != nullptr);

    // Inspect laid-out glyph fonts so relative sizes and the document's zoom
    // are resolved by Qt, including fonts inside caption table cells.
    const auto pixelSize = [&](const QString& text, bool backwards = false) -> qreal {
        QTextDocument* document = browser->document();
        document->documentLayout()->documentSize();
        const QTextCursor cursor = document->find(
            text, backwards ? document->characterCount() - 1 : 0,
            backwards ? QTextDocument::FindBackward | QTextDocument::FindWholeWords
                      : QTextDocument::FindFlags());
        if (cursor.isNull())
            return -1;
        const QTextBlock block = cursor.block();
        const auto runs = block.layout()->glyphRuns(cursor.selectionStart() - block.position(), 1);
        return runs.isEmpty() ? -1 : runs.first().rawFont().pixelSize();
    };
    const auto sizes = [&]() {
        QList<qreal> result { pixelSize(QStringLiteral("Index")), pixelSize(formula),
                              pixelSize(caption), pixelSize(variable, true) };
        if (page.startsWith(QStringLiteral("geometry/"))) {
            const QTextCursor image = browser->document()->find(QString(QChar::ObjectReplacementCharacter));
            if (image.isNull()) {
                result.append(-1);
            } else {
                const QTextBlock block = image.block();
                const int offset = image.selectionStart() - block.position();
                result.append(block.layout()->lineForTextPosition(offset).height());
            }
        }
        return result;
    };
    const QList<qreal> original = sizes();
    for (qreal size : original)
        QVERIFY(size > 0);
    const qreal originalBaseSize = browser->document()->defaultFont().pointSizeF();

    for (int steps : { 4, -4 }) {
        browser->zoomIn(steps);
        const qreal scale = browser->document()->defaultFont().pointSizeF() / originalBaseSize;
        const QList<qreal> zoomed = sizes();
        for (qsizetype i = 0; i < original.size(); ++i) {
            QVERIFY2(steps > 0 ? zoomed[i] > original[i] : zoomed[i] < original[i],
                     qPrintable(QStringLiteral("text sample %1 does not follow zoom").arg(i)));
            QVERIFY(qAbs(zoomed[i] / original[i] - scale) < 0.1);
        }
        if (!unit.isEmpty())
            QCOMPARE(pixelSize(unit), pixelSize(formula));

        // Reloading content must retain the same sizes at the current zoom.
        dock.openPage(QUrl(QStringLiteral("index")));
        dock.openPage(QUrl(page));
        QCOMPARE(sizes(), zoomed);
        dock.setContentSurfaceColors(QColor(steps > 0 ? "#202020" : "#303030"), QColor("#eeeeee"));
        QCOMPARE(sizes(), zoomed);
        dock.retranslateText();
        QCOMPARE(sizes(), zoomed);
        browser->zoomOut(steps);
        QCOMPARE(sizes(), original);
    }
    if (!unit.isEmpty())
        QCOMPARE(pixelSize(unit), pixelSize(formula));
}

void TestDockLayoutUi::restored_constants_dock_empty_filter_fills_header()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;
    settings->constantsDockDomain.clear();
    settings->constantsDockSubdomain.clear();
    settings->constantsDockSearchText = QStringLiteral("no-such-constant-filter-value");

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(searchBox != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(table->header() != nullptr);
    QCOMPARE(searchBox->text(), QStringLiteral("no-such-constant-filter-value"));
    QCOMPARE(table->topLevelItemCount(), 0);
    QTRY_VERIFY2(table->header()->length() >= table->header()->width() - 1,
                 qPrintable(QStringLiteral("length=%1 header=%2 sections=%3,%4,%5")
                                .arg(table->header()->length())
                                .arg(table->header()->width())
                                .arg(table->header()->sectionSize(0))
                                .arg(table->header()->sectionSize(1))
                                .arg(table->header()->sectionSize(2))));
}

void TestDockLayoutUi::dock_scroll_corner_uses_scrollbar_track_fill()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor base(QStringLiteral("#1f3229"));
    const QVector<QColor> shades = generateOklchShades(base, 6, ThemePolarity::Dark);
    const QColor expectedTrackFill = shades.at(UiConfig::DockBackgroundShade);

    MainWindow window;
    window.resize(700, 420);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(table != nullptr);

    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    for (int column = 0; column < table->columnCount(); ++column)
        table->setColumnWidth(column, 240);
    constantsDock->show();
    constantsDock->raise();
    window.resizeDocks(QList<QDockWidget*> { constantsDock },
                       QList<int> { UiConfig::ConstantsDockMinimumWidth },
                       Qt::Horizontal);
    QCoreApplication::processEvents();

    QWidget* corner = table->cornerWidget();
    QVERIFY(corner != nullptr);
    QCOMPARE(corner->palette().color(QPalette::Window).name(), expectedTrackFill.name());
    QVERIFY(corner->styleSheet().contains(expectedTrackFill.name()));
    QVERIFY(corner->styleSheet().contains(QStringLiteral("border: 0")));
    QTRY_VERIFY(table->horizontalScrollBar()->isVisible());
    QTRY_VERIFY(table->verticalScrollBar()->isVisible());
    QTRY_VERIFY(corner->isVisible());

    const QImage image = corner->grab().toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 1);
    QVERIFY(image.height() > 1);
    const QList<QPoint> samplePoints {
        QPoint(0, 0),
        QPoint(image.width() - 1, 0),
        QPoint(0, image.height() - 1),
        QPoint(image.width() - 1, image.height() - 1),
        image.rect().center()
    };
    for (const QPoint& point : samplePoints) {
        const QColor sampled = image.pixelColor(point);
        QVERIFY2(colorsAreClose(sampled, expectedTrackFill, 3),
                 qPrintable(QStringLiteral("corner sample %1,%2 is %3, expected %4")
                                .arg(point.x())
                                .arg(point.y())
                                .arg(sampled.name(), expectedTrackFill.name())));
    }
}

void TestDockLayoutUi::dock_separator_style_uses_primary_while_hovered_or_dragged()
{
    MainWindowStateGuard guard;
    guard.settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QColor normal(QStringLiteral("#123456"));
    const QColor primary(QStringLiteral("#abcdef"));

    QWidget propertyOwner;
    propertyOwner.setProperty("speedcrunchDockSeparatorNormalColor", normal);
    propertyOwner.setProperty("speedcrunchDockSeparatorActiveColor", primary);
    propertyOwner.resize(96, 64);
    QWidget styleHost(&propertyOwner);
    styleHost.resize(64, 32);
    // Painting reads the cursor in widget coordinates. Move this hidden test
    // widget instead of warping the desktop cursor, which Wayland forbids.
    const auto placeCursorAt = [&](const QPoint& position) {
        styleHost.move(propertyOwner.mapFromGlobal(QCursor::pos()) - position);
        QCOMPARE(styleHost.mapFromGlobal(QCursor::pos()), position);
    };

    placeCursorAt(QPoint(50, 24));
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_None).name(),
             normal.name());

    placeCursorAt(QPoint(4, 4));
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_None).name(),
             primary.name());
    QImage horizontalSeparator =
        dockSeparatorPrimitiveImage(&styleHost, QStyle::State_None, QSize(32, 8));
    const int horizontalStrokeTop =
        (horizontalSeparator.height() - UiConfig::DockSplitterStrokeWidth) / 2;
    const int horizontalStrokeBottom =
        horizontalStrokeTop + UiConfig::DockSplitterStrokeWidth - 1;
    for (int y = horizontalStrokeTop; y <= horizontalStrokeBottom; ++y)
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2, y).name(),
                 primary.name());
    if (horizontalStrokeTop > 0)
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2,
                                                horizontalStrokeTop - 1).alpha(), 0);
    if (horizontalStrokeBottom + 1 < horizontalSeparator.height())
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2,
                                                horizontalStrokeBottom + 1).alpha(), 0);

    QImage verticalSeparator =
        dockSeparatorPrimitiveImage(&styleHost, QStyle::State_None, QSize(8, 32));
    const int verticalStrokeLeft =
        (verticalSeparator.width() - UiConfig::DockSplitterStrokeWidth) / 2;
    const int verticalStrokeRight =
        verticalStrokeLeft + UiConfig::DockSplitterStrokeWidth - 1;
    for (int x = verticalStrokeLeft; x <= verticalStrokeRight; ++x)
        QCOMPARE(verticalSeparator.pixelColor(x, verticalSeparator.height() / 2).name(),
                 primary.name());
    if (verticalStrokeLeft > 0)
        QCOMPARE(verticalSeparator.pixelColor(verticalStrokeLeft - 1,
                                              verticalSeparator.height() / 2).alpha(), 0);
    if (verticalStrokeRight + 1 < verticalSeparator.width())
        QCOMPARE(verticalSeparator.pixelColor(verticalStrokeRight + 1,
                                              verticalSeparator.height() / 2).alpha(), 0);

    placeCursorAt(QPoint(4, 1));
    QImage thinHorizontalSeparator = dockSeparatorPrimitiveImage(&styleHost,
                                                                QStyle::State_None,
                                                                QRect(0, 0, 32, 1),
                                                                QSize(32, 3));
    QCOMPARE(thinHorizontalSeparator.pixelColor(thinHorizontalSeparator.width() / 2, 1).name(),
             primary.name());
    QCOMPARE(thinHorizontalSeparator.pixelColor(thinHorizontalSeparator.width() / 2, 2).alpha(),
             0);

    placeCursorAt(QPoint(1, 4));
    QImage thinVerticalSeparator = dockSeparatorPrimitiveImage(&styleHost,
                                                              QStyle::State_None,
                                                              QRect(0, 0, 1, 32),
                                                              QSize(3, 32));
    QCOMPARE(thinVerticalSeparator.pixelColor(1, thinVerticalSeparator.height() / 2).name(),
             primary.name());
    QCOMPARE(thinVerticalSeparator.pixelColor(2, thinVerticalSeparator.height() / 2).alpha(),
             0);

    placeCursorAt(QPoint(50, 24));
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_Sunken).name(),
             primary.name());
}

void TestDockLayoutUi::constants_dock_uses_configured_narrow_minimum_width()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->keypadMode,
        settings->keypadVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QCOMPARE(constantsDock->minimumWidth(), UiConfig::ConstantsDockMinimumWidth);

    QVERIFY(constantsDock->widget()->minimumSizeHint().width()
            <= UiConfig::ConstantsDockMinimumWidth);
}

void TestDockLayoutUi::lone_side_dock_opens_wide_enough_for_columns_data()
{
    QTest::addColumn<QString>("actionText");
    QTest::addColumn<QString>("dockName");
    QTest::addColumn<bool>("keypadVisible");
    for (const bool keypadVisible : {false, true}) {
        const char* keypad = keypadVisible ? "wide keypad" : "no keypad";
        QTest::addRow("constants, %s", keypad)
            << QStringLiteral("&Constants") << QStringLiteral("ConstantsDock") << keypadVisible;
        QTest::addRow("functions, %s", keypad)
            << QStringLiteral("&Functions") << QStringLiteral("FunctionsDock") << keypadVisible;
        QTest::addRow("units, %s", keypad)
            << QStringLiteral("Uni&ts") << QStringLiteral("UnitsDock") << keypadVisible;
    }
}

void TestDockLayoutUi::lone_side_dock_opens_wide_enough_for_columns()
{
    QFETCH(QString, actionText);
    QFETCH(QString, dockName);
    QFETCH(bool, keypadVisible);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->unitsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeScientificWide;
    settings->keypadVisible = keypadVisible;

    MainWindow window;
    // Leave the window room to grow, also on a small screen.
    const int screenWidth = window.screen()->availableGeometry().width();
    window.resize(qMin(1400, screenWidth - 100), 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // The wide keypad leaves no spare width in the main area.
    if (keypadVisible) {
        window.resize(window.minimumSizeHint().width(), 600);
        QTRY_COMPARE(window.width(), window.minimumSizeHint().width());
    }
    QMenu* viewMenu = menuWithTitle(window.menuBar(), QStringLiteral("&View"));
    QVERIFY(viewMenu != nullptr);
    QAction* action = nullptr;
    for (QAction* candidate : viewMenu->actions()) {
        if (candidate->text() == actionText)
            action = candidate;
    }
    QVERIFY(action != nullptr);

    const auto columnsWidth = [](const QTreeView* view) {
        const QAbstractItemView* itemView = view;
        int width = 0;
        for (int column = 0; column < view->header()->count(); ++column) {
            width += std::max(itemView->sizeHintForColumn(column),
                              view->header()->sectionSizeHint(column));
        }
        return width;
    };

    // Alone in its area, the dock grows until every column fits. The window
    // grows too, unless it already fills the screen.
    action->trigger();
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QVERIFY(dock != nullptr);
    QTreeWidget* list = dock->widget()->findChild<QTreeWidget*>();
    QVERIFY(list != nullptr);
    QTRY_VERIFY(list->model()->rowCount() > 0);
    const int availableWidth = window.screen()->availableGeometry().width();
    QTRY_VERIFY(list->viewport()->width() >= columnsWidth(list)
                || window.frameGeometry().width() >= availableWidth);

    // The cramped window cannot give History the width this check needs.
    if (keypadVisible)
        return;

    // Measure a width that can fit both docks and their tab bar.
    window.setHistoryDockVisible(true, false);
    QDockWidget* history = window.findChild<QDockWidget*>(QStringLiteral("HistoryDock"));
    QVERIFY(history != nullptr);
    QVERIFY(window.tabifiedDockWidgets(history).contains(dock));
    window.resizeDocks({history}, {1}, Qt::Horizontal);
    window.applyPendingDockSizes();
    const int tabbedWidth = history->width();

    action->trigger();
    QTRY_VERIFY(dock->isHidden());
    window.resizeDocks({history}, {tabbedWidth}, Qt::Horizontal);
    window.applyPendingDockSizes();
    const int historyWidth = history->width();
    // Make a column wider than the shared area on every platform.
    list->headerItem()->setSizeHint(0, QSize(historyWidth + 1, list->header()->height()));

    // A dock that joins an open area keeps the width the user chose.
    action->trigger();
    QTRY_VERIFY(!dock->isHidden());
    QTest::qWait(50);
    QCOMPARE(dock->width(), historyWidth);
    QVERIFY(list->viewport()->width() < columnsWidth(list));
}

void TestDockLayoutUi::lone_formula_book_opens_wide_enough_for_page()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->formulaBookActivePage.clear();
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadVisible = false;

    MainWindow window;
    // Leave the window room to grow, also on a small screen. On Wayland, the
    // compositor can send this first size again while the book opens, and a
    // window that already fills the screen cannot grow for the page.
    const int screenWidth = window.screen()->availableGeometry().width();
    window.resize(qMin(1400, screenWidth - 100), 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // At its minimum width, the window leaves no spare width for the book.
    window.resize(window.minimumSizeHint().width(), 600);
    QTRY_COMPARE(window.width(), window.minimumSizeHint().width());
    QMenu* viewMenu = menuWithTitle(window.menuBar(), QStringLiteral("&View"));
    QVERIFY(viewMenu != nullptr);
    QAction* action = nullptr;
    for (QAction* candidate : viewMenu->actions()) {
        if (candidate->text() == QStringLiteral("Formula &Book"))
            action = candidate;
    }
    QVERIFY(action != nullptr);

    // Alone in its area, the book grows until its page needs no horizontal
    // scrolling. The window grows too, unless it already fills the screen.
    action->trigger();
    QDockWidget* dock = window.findChild<QDockWidget*>(QStringLiteral("BookDock"));
    QVERIFY(dock != nullptr);
    QTextBrowser* browser = dock->findChild<QTextBrowser*>();
    QVERIFY(browser != nullptr);
    const int availableWidth = window.screen()->availableGeometry().width();
    QTRY_VERIFY2(browser->horizontalScrollBar()->maximum() == 0
                     || window.frameGeometry().width() >= availableWidth,
                 qPrintable(QStringLiteral("scroll=%1 viewport=%2 dock=%3 window=%4x%5 frame=%6 available=%7")
                                .arg(browser->horizontalScrollBar()->maximum())
                                .arg(browser->viewport()->width())
                                .arg(dock->width())
                                .arg(window.width())
                                .arg(window.height())
                                .arg(window.frameGeometry().width())
                                .arg(availableWidth)));
}

void TestDockLayoutUi::kept_window_size_handles_native_resize_requests_data()
{
    QTest::addColumn<QSize>("requestedSize");
    QTest::addColumn<bool>("expired");
    QTest::addColumn<bool>("restore");
    QTest::newRow("kept-size") << QSize(900, 600) << false << false;
    QTest::newRow("previous-size") << QSize(640, 600) << false << true;
    QTest::newRow("previous-size-raised-to-minimum") << QSize(700, 600) << false << true;
    QTest::newRow("unrecorded-startup-size") << QSize(640, 480) << false << true;
    QTest::newRow("height-below-minimum") << QSize(900, 380) << false << true;
    QTest::newRow("new-user-size") << QSize(760, 520) << false << false;
    QTest::newRow("expired-guard") << QSize(640, 480) << true << false;
}

void TestDockLayoutUi::kept_window_size_handles_native_resize_requests()
{
    QFETCH(QSize, requestedSize);
    QFETCH(bool, expired);
    QFETCH(bool, restore);
    MainWindowStateGuard guard;
    MainWindow window;
    window.setMinimumSize(700, 400);
    window.resize(900, 600);
    window.keepWindowSize(MainWindow::KeptWindowSize::UndoOlderSizes);
    // The dock grows after this earlier size, while a startup size with a
    // different height can still arrive from the compositor.
    window.m_replacedWindowSizes = {QSize(640, 600)};
    if (expired)
        window.m_keptWindowSizeDeadline = QDeadlineTimer(0);
    QCOMPARE(window.shouldRestoreKeptWindowSize(requestedSize), restore);
}

void TestDockLayoutUi::shown_dock_keeps_window_size_from_window_system()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeScientificWide;
    settings->keypadVisible = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.resize(window.minimumSizeHint().width(), 600);
    QTRY_COMPARE(window.width(), window.minimumSizeHint().width());
    const QSize oldSize = window.size();
    QMenu* viewMenu = menuWithTitle(window.menuBar(), QStringLiteral("&View"));
    QVERIFY(viewMenu != nullptr);
    QAction* action = nullptr;
    for (QAction* candidate : viewMenu->actions()) {
        if (candidate->text() == QStringLiteral("Formula &Book"))
            action = candidate;
    }
    QVERIFY(action != nullptr);

    // The window grows for the dock and its page.
    action->trigger();
    QDockWidget* dock = window.findChild<QDockWidget*>(QStringLiteral("BookDock"));
    QVERIFY(dock != nullptr);
    QTextBrowser* browser = dock->findChild<QTextBrowser*>();
    QVERIFY(browser != nullptr);
    QTRY_VERIFY(window.width() > oldSize.width());
    // The window grows in steps. The last one ends when the page needs no
    // horizontal scrolling, or when the window fills the screen.
    const int availableWidth = window.screen()->availableGeometry().width();
    QTRY_VERIFY(browser->horizontalScrollBar()->maximum() == 0
                || window.frameGeometry().width() >= availableWidth);
    QTest::qWait(50);
    const QSize grownSize = window.size();
    const int dockWidth = dock->width();

    // On Wayland, the compositor can send the old size after the View menu
    // closes, and Qt applies it. That would leave the window narrower than
    // the keypad and the dock need. The dock also gets its width back, and
    // does not leave that space to the editor. Window systems such as Windows
    // and Openbox raise the old size to the current minimum size, so send that.
    class LeftSizeWatcher : public QObject {
    public:
        explicit LeftSizeWatcher(const QSize& size) : m_size(size) {}
        bool left = false;
        bool eventFilter(QObject*, QEvent* event) override
        {
            if (event->type() == QEvent::Resize && static_cast<QResizeEvent*>(event)->size() != m_size)
                left = true;
            return false;
        }

    private:
        QSize m_size;
    } watcher(grownSize);
    window.installEventFilter(&watcher);
    window.windowHandle()->resize(oldSize.expandedTo(window.minimumSize()));
    // On Windows and X11, the old size comes some time later. Wait for it, so
    // the check below sees the size after the undo, not the size before it.
    QTRY_VERIFY(watcher.left);
    window.removeEventFilter(&watcher);
    QTRY_COMPARE(window.size(), grownSize);
    QTest::qWait(50);
    QCOMPARE(window.size(), grownSize);
    QCOMPARE(dock->width(), dockWidth);
    QVERIFY(window.width() >= window.minimumSizeHint().width());

    // A new size, for example from the user, stays.
    const QSize userSize = grownSize + QSize(40, 0);
    window.windowHandle()->resize(userSize);
    QTRY_COMPARE(window.size(), userSize);
    QTest::qWait(50);
    QCOMPARE(window.size(), userSize);
}

void TestDockLayoutUi::fitted_and_kept_dock_widths_apply_at_once()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->formulaBookActivePage.clear();
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadVisible = false;

    MainWindow window;
    const int screenWidth = window.screen()->availableGeometry().width();
    window.resize(qMin(1400, screenWidth - 100), 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.setFormulaBookDockVisible(true, false);
    QDockWidget* dock = window.findChild<QDockWidget*>(QStringLiteral("BookDock"));
    QVERIFY(dock != nullptr);
    QTextBrowser* browser = dock->findChild<QTextBrowser*>();
    QVERIFY(browser != nullptr);
    // The book is too narrow for its page.
    window.resizeDocks({dock}, {dock->minimumSizeHint().width()}, Qt::Horizontal);
    QTRY_VERIFY(browser->horizontalScrollBar()->maximum() > 0);
    // The window has room for the page, also on a small screen.
    const int roomyWidth = window.minimumSizeHint().width()
        + browser->horizontalScrollBar()->maximum() + 100;
    if (window.width() < roomyWidth) {
        window.resize(roomyWidth, window.height());
        QTRY_COMPARE(window.width(), roomyWidth);
    }
    QVERIFY(browser->horizontalScrollBar()->maximum() > 0);
    const int narrowWidth = dock->width();

    // On Wayland, an older window size can arrive before the next layout pass.
    // The window then keeps the dock sizes of that moment and restores them
    // with the kept window size. So the last fit step must widen the book at
    // once. Run only that step, without the layout pass after it.
    window.fitDockToColumns(dock, window.dockWidgetArea(dock),
                            MainWindow::DockFitStep::FinalResizeDock);
    QCoreApplication::sendPostedEvents(&window, QEvent::MetaCall);
    QCOMPARE(browser->horizontalScrollBar()->maximum(), 0);

    // Restored dock widths also apply at once, in case another older size
    // follows.
    const int fittedWidth = dock->width();
    QVERIFY(fittedWidth > narrowWidth);
    // Do not run the event loop here: a size from the window system would
    // make the window remember the dock sizes again.
    window.rememberKeptDockSizes();
    window.resizeDocks({dock}, {narrowWidth}, Qt::Horizontal);
    window.applyPendingDockSizes();
    QVERIFY(dock->width() < fittedWidth);
    window.restoreKeptDockSizes();
    QCOMPARE(dock->width(), fittedWidth);
    QCOMPARE(browser->horizontalScrollBar()->maximum(), 0);
}

void TestDockLayoutUi::view_dock_menu_tracks_and_changes_only_active_window()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->unitsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow;
    firstWindow.resize(800, 500);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

    MainWindow secondWindow;
    secondWindow.resize(800, 500);

    struct DockSpec {
        const char* setter;
        const char* objectName;
        const char* actionText;
        bool hasFocusArgument;
    };
    const DockSpec dockSpecs[] = {
        { "setFormulaBookDockVisible", "BookDock", "Formula &Book", true },
        { "setConstantsDockVisible", "ConstantsDock", "&Constants", true },
        { "setFunctionsDockVisible", "FunctionsDock", "&Functions", true },
        { "setUnitsDockVisible", "UnitsDock", "Uni&ts", true },
        { "setVariablesDockVisible", "VariablesDock", "User &Variables", true },
        { "setUserFunctionsDockVisible", "UserFunctionsDock", "Use&r Functions", true },
        { "setUserUnitsDockVisible", "UserUnitsDock", "User &Units", true },
        { "setHistoryDockVisible", "HistoryDock", "&History", true },
        { "setBitfieldVisible", "BitfieldDock", "Bitfield", false }
    };
    const auto setDockVisible = [](MainWindow* window, const DockSpec& spec, bool visible) {
        if (!spec.hasFocusArgument) {
            return QMetaObject::invokeMethod(window, spec.setter, Qt::DirectConnection,
                                             Q_ARG(bool, visible));
        }
        return QMetaObject::invokeMethod(window, spec.setter, Qt::DirectConnection,
                                         Q_ARG(bool, visible), Q_ARG(bool, false));
    };
    const auto dockIsVisible = [](MainWindow* window, const DockSpec& spec) {
        QDockWidget* dock = window->findChild<QDockWidget*>(QString::fromLatin1(spec.objectName));
        return dock != nullptr && !dock->isHidden();
    };

    QMenu* firstViewMenu = menuWithTitle(firstWindow.menuBar(), QStringLiteral("&View"));
    QMenu* secondViewMenu = menuWithTitle(secondWindow.menuBar(), QStringLiteral("&View"));
    QVERIFY(firstViewMenu != nullptr);
    QVERIFY(secondViewMenu != nullptr);

    for (const DockSpec& spec : dockSpecs) {
        QVERIFY(setDockVisible(&firstWindow, spec, true));
        QVERIFY(dockIsVisible(&firstWindow, spec));
        QVERIFY(!dockIsVisible(&secondWindow, spec));
    }

    firstWindow.raise();
    if (!UiTestFixture::activateWindow(&firstWindow))
        return;
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
    QEvent firstWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    for (const DockSpec& spec : dockSpecs) {
        QCOMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
        QAction* firstAction = directMenuActionWithText(
            firstViewMenu, QString::fromLatin1(spec.actionText));
        QAction* secondAction = directMenuActionWithText(
            secondViewMenu, QString::fromLatin1(spec.actionText));
        QVERIFY(firstAction != nullptr);
        QVERIFY(secondAction != nullptr);
        QTRY_VERIFY2(firstAction->isChecked(), spec.objectName);
        QTRY_VERIFY2(secondAction->isChecked(), spec.objectName);

        // Simulate the native menu dispatching the inactive window's action.
        secondAction->trigger();
        QCoreApplication::processEvents();
        QVERIFY(!dockIsVisible(&firstWindow, spec));
        QVERIFY(!dockIsVisible(&secondWindow, spec));
        QVERIFY(!firstAction->isChecked());
        QVERIFY(!secondAction->isChecked());
    }

    for (const DockSpec& spec : dockSpecs) {
        QVERIFY(setDockVisible(&secondWindow, spec, true));
        QVERIFY(!dockIsVisible(&firstWindow, spec));
        QVERIFY(dockIsVisible(&secondWindow, spec));
    }

    // Show the second window when its turn starts so it gets native focus on map.
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));
    secondWindow.raise();
    if (!UiTestFixture::activateWindow(&secondWindow))
        return;
    secondWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&secondWindow));
    QEvent secondWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&secondWindow, &secondWindowActivate);
    QCoreApplication::processEvents();

    for (const DockSpec& spec : dockSpecs) {
        QCOMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&secondWindow));
        QAction* firstAction = directMenuActionWithText(
            firstViewMenu, QString::fromLatin1(spec.actionText));
        QAction* secondAction = directMenuActionWithText(
            secondViewMenu, QString::fromLatin1(spec.actionText));
        QVERIFY(firstAction != nullptr);
        QVERIFY(secondAction != nullptr);
        QTRY_VERIFY2(firstAction->isChecked(), spec.objectName);
        QTRY_VERIFY2(secondAction->isChecked(), spec.objectName);

        firstAction->trigger();
        QCoreApplication::processEvents();
        QVERIFY(!dockIsVisible(&firstWindow, spec));
        QVERIFY(!dockIsVisible(&secondWindow, spec));
        QVERIFY(!firstAction->isChecked());
        QVERIFY(!secondAction->isChecked());
    }
}

void TestDockLayoutUi::closing_and_reopening_docks_keeps_attached_widgets()
{
    constexpr int dockLayoutStateVersion = 1;
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        QByteArray oldWindowState;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->windowState = oldWindowState;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->windowState,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    appSettings->sessionLayoutJson.clear();
    appSettings->windowState.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->hasNumberFormatStyleSetting = true;

    QByteArray legacyDockState;
    QByteArray populatedDockState;
    {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        struct DockSpec {
            const char* setter;
            const char* objectName;
            bool hasFocusArgument;
        };
        const DockSpec dockSpecs[] = {
            { "setBitfieldVisible", "BitfieldDock", false },
            { "setFormulaBookDockVisible", "BookDock", true },
            { "setConstantsDockVisible", "ConstantsDock", true },
            { "setFunctionsDockVisible", "FunctionsDock", true },
            { "setHistoryDockVisible", "HistoryDock", true },
            { "setVariablesDockVisible", "VariablesDock", true },
            { "setUserFunctionsDockVisible", "UserFunctionsDock", true },
            { "setUserUnitsDockVisible", "UserUnitsDock", true }
        };

        const auto invokeVisible = [&window](const DockSpec& spec, bool visible) {
            if (!spec.hasFocusArgument) {
                return QMetaObject::invokeMethod(&window, spec.setter, Qt::DirectConnection,
                                                 Q_ARG(bool, visible));
            }
            return QMetaObject::invokeMethod(&window, spec.setter, Qt::DirectConnection,
                                             Q_ARG(bool, visible), Q_ARG(bool, false));
        };

        const auto dockCount = [&window](const QString& objectName) {
            int count = 0;
            for (QDockWidget* dock : window.findChildren<QDockWidget*>()) {
                if (dock->objectName() == objectName)
                    ++count;
            }
            return count;
        };

        for (const DockSpec& spec : dockSpecs) {
            const QString objectName = QString::fromLatin1(spec.objectName);
            QPointer<QDockWidget> originalDock = window.findChild<QDockWidget*>(objectName);
            QVERIFY(originalDock == nullptr);
            QCOMPARE(dockCount(objectName), 0);

            QVERIFY(invokeVisible(spec, true));
            QCoreApplication::processEvents();
            originalDock = window.findChild<QDockWidget*>(objectName);
            QVERIFY(originalDock != nullptr);
            QCOMPARE(dockCount(objectName), 1);

            originalDock->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();

            QVERIFY(originalDock != nullptr);
            QVERIFY(!originalDock->isVisible());
            QVERIFY(window.dockWidgetArea(originalDock) != Qt::NoDockWidgetArea);
            QCOMPARE(dockCount(objectName), 1);

            const QByteArray hiddenState = window.saveState(dockLayoutStateVersion);
            QVERIFY(window.restoreState(hiddenState, dockLayoutStateVersion));
            QVERIFY(invokeVisible(spec, true));
            QCoreApplication::processEvents();
            QCOMPARE(window.findChild<QDockWidget*>(objectName), originalDock.data());
            QCOMPARE(dockCount(objectName), 1);
        }

        legacyDockState = window.saveState();
        populatedDockState = window.saveState(dockLayoutStateVersion);
    }

    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;

    {
        appSettings->windowState = legacyDockState;
        MainWindow legacyStateWindow;
        legacyStateWindow.show();
        QCoreApplication::processEvents();

        QDockWidget* bitfield = legacyStateWindow.findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
        QVERIFY(bitfield == nullptr);
    }

    {
        appSettings->windowState = populatedDockState;
        MainWindow restoredWindow;
        restoredWindow.show();
        QCoreApplication::processEvents();

        const char* dockNames[] = {
            "BitfieldDock", "BookDock", "ConstantsDock", "FunctionsDock",
            "HistoryDock", "VariablesDock", "UserFunctionsDock", "UserUnitsDock"
        };
        for (const char* dockName : dockNames)
            QCOMPARE(restoredWindow.findChildren<QDockWidget*>(QString::fromLatin1(dockName)).size(), 1);
        QDockWidget* bitfield = restoredWindow.findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
        QVERIFY(bitfield != nullptr);
        QVERIFY(bitfield->isVisible());
    }
}

void TestDockLayoutUi::formula_book_ignores_repeated_appearance_events_data()
{
    QTest::addColumn<bool>("system");
    QTest::newRow("theme") << false;
    QTest::newRow("system") << true;
}

void TestDockLayoutUi::formula_book_ignores_repeated_appearance_events()
{
    QFETCH(bool, system);
    MainWindowStateGuard guard;
    guard.settings->dockAppearance = system ? Settings::DockAppearanceSystem
                                            : Settings::DockAppearanceSpeedCrunch;
    BookDock dock;
    dock.resize(400, 240);
    dock.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dock));
    dock.openPage(QUrl(QStringLiteral("electronics/ohmslaw")));
    QCoreApplication::processEvents();
    auto* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser);
    QTextCursor selection = browser->document()->find(QStringLiteral("Index"));
    QVERIFY(!selection.isNull());
    browser->setTextCursor(selection);
    const QString selectedText = browser->textCursor().selectedText();
    const int revision = browser->document()->revision();
    const int scroll = browser->verticalScrollBar()->value();
    QSignalSpy contents(browser->document(), &QTextDocument::contentsChanged);

    for (int i = 0; i < 12; ++i) {
        for (QWidget* widget : {static_cast<QWidget*>(&dock), static_cast<QWidget*>(browser)}) {
            QEvent style(QEvent::StyleChange);
            QCoreApplication::sendEvent(widget, &style);
            QEvent palette(QEvent::PaletteChange);
            QCoreApplication::sendEvent(widget, &palette);
        }
    }
    QCoreApplication::processEvents();

    QCOMPARE(contents.count(), 0);
    QCOMPARE(browser->document()->revision(), revision);
    QCOMPARE(browser->textCursor().selectedText(), selectedText);
    QCOMPARE(browser->verticalScrollBar()->value(), scroll);
    QCOMPARE(dock.currentPage(), QStringLiteral("electronics/ohmslaw"));
}

void TestDockLayoutUi::formula_book_text_requests_single_font_family()
{
    // A family list makes Qt populate every installed family when the text is
    // laid out, which dominated startup on Windows systems with many fonts.
    MainWindowStateGuard guard;
    BookDock dock;
    auto* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser);
    for (const QString& page : {QStringLiteral("index"), QStringLiteral("electronics/ohmslaw"),
                                QStringLiteral("geometry/sphere")}) {
        dock.openPage(QUrl(page));
        QCoreApplication::processEvents();
        for (QTextBlock block = browser->document()->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QStringList families = it.fragment().charFormat().fontFamilies().toStringList();
                QVERIFY2(families.size() <= 1,
                         qPrintable(page + QStringLiteral(": ") + families.join(QStringLiteral(", "))));
            }
        }
    }
}

void TestDockLayoutUi::formula_book_covers_geometry_catalog()
{
    BookDock dock;
    auto* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser);
    const auto pageLinks = [browser]() {
        QSet<QString> links;
        for (QTextBlock block = browser->document()->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QString href = it.fragment().charFormat().anchorHref();
                if (!href.isEmpty())
                    links.insert(href);
            }
        }
        return links;
    };

    QSet<QString> functions;
    const QSet<QString> indexLinks = pageLinks();
    for (const QString& chapter : {QStringLiteral("geometry/2d"), QStringLiteral("geometry/3d")}) {
        QVERIFY2(indexLinks.contains(chapter), qPrintable(chapter));
        dock.openPage(QUrl(chapter));
        QSet<QString> shapes = pageLinks();
        QVERIFY(shapes.remove(QStringLiteral("index")));
        QVERIFY(!shapes.isEmpty());
        for (const QString& shape : shapes) {
            dock.openPage(QUrl(shape));
            QTextDocument* document = browser->document();
            const QString replacement(QChar::ObjectReplacementCharacter);
            const QTextCursor diagram = document->find(replacement);
            QVERIFY2(!diagram.isNull(), qPrintable(shape));
            QVERIFY(document->find(replacement, diagram.selectionEnd()).isNull());
            const QTextCharFormat format = diagram.charFormat();
            QVERIFY2(format.objectType() >= QTextFormat::UserObject, qPrintable(shape));
            QVERIFY2(!format.stringProperty(QTextFormat::ImageAltText).isEmpty(), qPrintable(shape));
            auto* renderer = document->documentLayout()->handlerForObject(format.objectType());
            QVERIFY(renderer);
            const int position = diagram.selectionStart();
            const QSize size = renderer->intrinsicSize(document, position, format).toSize();
            QVERIFY(size.width() > 0 && size.height() > 0);
            QImage blank(size, QImage::Format_ARGB32_Premultiplied);
            blank.fill(Qt::transparent);
            QImage drawing = blank;
            QPainter painter(&drawing);
            renderer->drawObject(&painter, QRectF(drawing.rect()), document, position, format);
            painter.end();
            QVERIFY2(drawing != blank, qPrintable(shape));

            const QSet<QString> links = pageLinks();
            QVERIFY2(links.contains(chapter) && links.contains(QStringLiteral("index")), qPrintable(shape));
            const qsizetype count = functions.size();
            for (const QString& href : links) {
                const QUrl url(href);
                if (url.scheme() != QLatin1String("function"))
                    continue;
                QVERIFY2(!functions.contains(url.path()), qPrintable(href));
                functions.insert(url.path());
            }
            QVERIFY2(functions.size() > count, qPrintable(shape));
        }
    }

    QSet<QString> geometryFunctions;
    const FunctionRepo* repo = FunctionRepo::instance();
    for (const QString& identifier : repo->getListedIdentifiers()) {
        if (repo->find(identifier)->domainType() == FunctionDomain::Geometry)
            geometryFunctions.insert(identifier);
    }
    QCOMPARE(functions, geometryFunctions);
}

void TestDockLayoutUi::formula_book_inserts_geometry_function_calls()
{
    MainWindowStateGuard guard;
    guard.settings->formulaBookDockVisible = true;
    guard.settings->formulaBookActivePage = QStringLiteral("index");
    guard.settings->sessionLayoutJson.clear();
    guard.settings->windowState.clear();
    MainWindow window(false);
    window.resize(900, 650);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* dock = window.findChild<BookDock*>();
    auto* editor = window.findChild<Editor*>();
    QVERIFY(dock);
    QVERIFY(editor);
    auto* browser = dock->findChild<QTextBrowser*>();
    QVERIFY(browser);
    const auto clickLink = [browser](const QString& href) {
        for (QTextBlock block = browser->document()->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                if (fragment.charFormat().anchorHref() != href)
                    continue;
                QTextCursor cursor(block);
                cursor.setPosition(fragment.position() + fragment.length() / 2);
                browser->setTextCursor(cursor);
                browser->ensureCursorVisible();
                const QPoint point = browser->cursorRect(cursor).center();
                QCOMPARE(browser->anchorAt(point), href);
                QTest::mouseClick(browser->viewport(), Qt::LeftButton, Qt::NoModifier, point);
                return;
            }
        }
        QFAIL(qPrintable(href));
    };

    clickLink(QStringLiteral("geometry/3d"));
    QCOMPARE(dock->currentPage(), QStringLiteral("geometry/3d"));
    clickLink(QStringLiteral("geometry/cylinder"));
    QCOMPARE(dock->currentPage(), QStringLiteral("geometry/cylinder"));
    editor->setText(QStringLiteral("5 + "));
    editor->moveCursor(QTextCursor::End);
    clickLink(QStringLiteral("function:volume_cylinder"));
    QCOMPARE(editor->text(), QStringLiteral("5 + volume_cylinder()"));
    QCOMPARE(editor->textCursor().position(), editor->text().size() - 1);
    QCOMPARE(dock->currentPage(), QStringLiteral("geometry/cylinder"));
}

void TestDockLayoutUi::functions_dock_lists_cbrt_without_its_symbol()
{
    MainWindowStateGuard guard;
    FunctionsWidget functions;
    auto* list = functions.findChild<QTreeWidget*>();
    QVERIFY(list);
    QStringList identifiers;
    for (int i = 0; i < list->topLevelItemCount(); ++i)
        identifiers << list->topLevelItem(i)->text(0);
    QVERIFY(identifiers.contains(QStringLiteral("cbrt")));
    QVERIFY(!identifiers.contains(QString::fromUtf8("∛")));
}

void TestDockLayoutUi::formula_book_combines_palette_changes()
{
    MainWindowStateGuard guard;
    guard.settings->dockAppearance = Settings::DockAppearanceSpeedCrunch;
    BookDock dock;
    dock.openPage(QUrl(QStringLiteral("electronics/ohmslaw")));
    QCoreApplication::processEvents();
    auto* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser);
    const QString text = browser->toPlainText();
    browser->zoomIn(3);
    const QFont font = browser->document()->defaultFont();
    QSignalSpy contents(browser->document(), &QTextDocument::contentsChanged);

    dock.setContentSurfaceColors(QColor("#202020"), QColor("#dddddd"));
    dock.setContentSurfaceColors(QColor("#303030"), QColor("#eeeeee"));
    dock.setContentSurfaceColors(QColor("#404040"), QColor("#ffffff"));
    QCOMPARE(contents.count(), 0);
    QCoreApplication::processEvents();

    QVERIFY(contents.count() > 0);
    QVERIFY2(contents.count() <= 2, "A palette burst should rebuild the document once");
    QCOMPARE(browser->palette().color(QPalette::Base), QColor("#404040"));
    QCOMPARE(browser->palette().color(QPalette::Text), QColor("#ffffff"));
    QCOMPARE(browser->document()->defaultFont(), font);
    QCOMPARE(browser->toPlainText(), text);
    QCOMPARE(dock.currentPage(), QStringLiteral("electronics/ohmslaw"));

    // A pending appearance refresh must not bring back the previous page.
    dock.setContentSurfaceColors(QColor("#505050"), QColor("#eeeeee"));
    dock.openPage(QUrl(QStringLiteral("index")));
    QCOMPARE(dock.currentPage(), QStringLiteral("index"));
    const QString indexText = browser->toPlainText();
    QCoreApplication::processEvents();
    QCOMPARE(browser->toPlainText(), indexText);
}

void TestDockLayoutUi::startup_styling_keeps_formula_book_document()
{
    MainWindowStateGuard guard;
    guard.settings->dockAppearance = Settings::DockAppearanceSpeedCrunch;
    guard.settings->formulaBookDockVisible = true;
    guard.settings->sessionLayoutJson.clear();
    guard.settings->windowState.clear();
    MainWindow window(false);
    auto* dock = window.findChild<BookDock*>(QStringLiteral("BookDock"));
    QVERIFY(dock);
    dock->hide();
    auto* browser = dock->findChild<QTextBrowser*>();
    QVERIFY(browser);
    QSignalSpy contents(browser->document(), &QTextDocument::contentsChanged);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();
    QVERIFY2(contents.count() <= 2, "Startup should render the final book palette once");
    QVERIFY(!browser->toPlainText().isEmpty());
    QVERIFY(dock->isHidden());
    const int settledCount = contents.count();
    QCoreApplication::processEvents();
    QCOMPARE(contents.count(), settledCount);
    QCOMPARE(window.findChildren<QDockWidget*>().size(), 1);
}

void TestDockLayoutUi::formula_book_retranslation_refreshes_cached_page()
{
    class BookTranslator : public QTranslator {
    public:
        QString translate(const char* context, const char* source,
                          const char* = nullptr, int = -1) const override
        {
            if (qstrcmp(context, "Book") == 0 && qstrcmp(source, "Index") == 0)
                return QStringLiteral("Translated book index");
            return QString();
        }
    } translator;
    BookDock dock;
    dock.openPage(QUrl(QStringLiteral("electronics/ohmslaw")));
    QCoreApplication::processEvents();
    auto* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser);
    QVERIFY(!browser->toPlainText().contains(QStringLiteral("Translated book index")));
    QCoreApplication::installTranslator(&translator);
    const auto removeTranslator = qScopeGuard([&]() {
        QCoreApplication::removeTranslator(&translator);
    });
    dock.retranslateText();
    QVERIFY(browser->toPlainText().contains(QStringLiteral("Translated book index")));
    QCOMPARE(dock.currentPage(), QStringLiteral("electronics/ohmslaw"));
    QCoreApplication::processEvents();
    QVERIFY(browser->toPlainText().contains(QStringLiteral("Translated book index")));
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestDockLayoutUi>(argc, argv);
}

#include "testdocklayoutui.moc"
