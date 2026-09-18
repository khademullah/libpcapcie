#pragma once

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTableView>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextBrowser>
#include <QTextEdit>
#include <QSplitter>
#include <QTreeWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void loadTraceFile(const QString &path);
    void openTraceDialog();

private slots:
    void openTrace();
    void enumeratePciDevice();
    void openAiPerfDialog();
    void applyFilter();
    void showPacketDetails();
    void updateSummaryStats();

private:
    void loadCsv(const QString &path);
    QStringList splitCsvLine(const QString &line) const;

    QTableView *tableView;
    QTextEdit *detailsView;
    QTreeWidget *decodeTree;
    QSplitter *splitter;
    QStandardItemModel *model;
    QComboBox *typeFilter;
    QComboBox *directionFilter;
    QComboBox *backendFilter;
    QLineEdit *deviceIdBox;
    QLineEdit *scenarioBox;
    QLineEdit *searchBox;
    QPushButton *openButton;
    QPushButton *enumerateButton;
    QPushButton *aiPerfButton;
    QPushButton *themeButton;
    QLabel *statusLabel;
    QLabel *totalLabel;
    QLabel *txLabel;
    QLabel *rxLabel;
    QLabel *filteredLabel;
    bool darkMode;
    void applyTheme();
    void colorRows();
    void runAiPerformanceScenario(const QString &profile,
                                 int rootPorts,
                                 int endpointsPerRoot,
                                 int iterations,
                                 int latencyNs,
                                 int tps,
                                 int burstSize,
                                 int jitterNs,
                                 double dropRate,
                                 int busCount);
};
