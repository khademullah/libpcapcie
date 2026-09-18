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
#include <QTextEdit>
#include <QSplitter>
#include <QTreeWidget>

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
    QLineEdit *searchBox;
    QPushButton *openButton;
    QPushButton *enumerateButton;
    QPushButton *themeButton;
    QLabel *statusLabel;
    QLabel *totalLabel;
    QLabel *txLabel;
    QLabel *rxLabel;
    QLabel *filteredLabel;
    bool darkMode;
    void applyTheme();
    void colorRows();
};
