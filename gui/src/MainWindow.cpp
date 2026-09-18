#include "MainWindow.h"

#include "pcapcie/pcapcie.h"
#include "pcapcie/tlp.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QDebug>
#include <QDateTime>

namespace {

QString normalizeDirection(const QString &value)
{
    QString v = value.trimmed().toUpper();
    if (v == "TX" || v == "1") return "TX";
    if (v == "RX" || v == "2") return "RX";
    return v;
}

QString decodeTypeName(const QString &value)
{
    const QString v = value.trimmed();
    if (v == "0") return "MemRd";
    if (v == "1") return "MemWr";
    if (v == "4") return "CfgRd";
    if (v == "5") return "CfgWr";
    if (v == "10") return "Cpl";
    return v.isEmpty() ? "Unknown" : v;
}

QString sanitizeType(const QString &value)
{
    return decodeTypeName(value);
}

QString formatHexDump(const QString &payload)
{
    QString normalized = payload.trimmed();
    normalized.remove(' ');
    normalized.remove('\t');
    normalized.remove('\n');
    normalized.remove('\r');

    if (normalized.isEmpty()) {
        return "<no payload>";
    }

    QByteArray bytes = QByteArray::fromHex(normalized.toLatin1());
    if (bytes.isEmpty() && !normalized.isEmpty()) {
        return QString("%1\n<not valid hex payload>").arg(normalized);
    }

    QStringList lines;
    for (int i = 0; i < bytes.size(); i += 16) {
        QByteArray chunk = bytes.mid(i, 16);
        QString hex;
        QString ascii;
        for (int j = 0; j < chunk.size(); ++j) {
            const unsigned char value = static_cast<unsigned char>(chunk.at(j));
            hex += QString("%1 ").arg(value, 2, 16, QLatin1Char('0'));
            ascii += (value >= 0x20 && value <= 0x7e) ? QChar(value) : '.';
        }
        while (hex.size() < 48) {
            hex += "   ";
        }
        lines << QString("%1  %2  %3").arg(i, 4, 16, QLatin1Char('0')).arg(hex).arg(ascii);
    }
    return lines.join("\n");
}

QString pciVendorName(uint16_t vendorId)
{
    switch (vendorId) {
        case 0x10EC: return "Realtek";
        case 0x8086: return "Intel";
        case 0x14E4: return "Broadcom";
        case 0x1AF4: return "Red Hat, Inc.";
        case 0x10DE: return "NVIDIA";
        case 0x10B5: return "PLX";
        default: return "Unknown vendor";
    }
}

QString pciClassName(uint32_t classCode)
{
    switch ((classCode >> 8) & 0xFFFFFF) {
        case 0x020000: return "Ethernet controller";
        case 0x010000: return "SCSI controller";
        case 0x0C0300: return "USB controller";
        case 0x060000: return "Bridge device";
        case 0x030000: return "VGA compatible controller";
        default: return "Unknown class";
    }
}

QString makeHexLabel(uint32_t value, int width)
{
    return QString("0x%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    QHBoxLayout *toolbar = new QHBoxLayout();

    darkMode = false;

    openButton = new QPushButton("Open trace", this);
    enumerateButton = new QPushButton("Enumerate PCI", this);
    themeButton = new QPushButton("Dark", this);
    typeFilter = new QComboBox(this);
    directionFilter = new QComboBox(this);
    backendFilter = new QComboBox(this);
    deviceIdBox = new QLineEdit(this);
    searchBox = new QLineEdit(this);
    statusLabel = new QLabel("No trace loaded", this);
    totalLabel = new QLabel("Total: 0", this);
    txLabel = new QLabel("TX: 0", this);
    rxLabel = new QLabel("RX: 0", this);
    filteredLabel = new QLabel("Visible: 0", this);

    typeFilter->addItem("All types");
    typeFilter->addItem("CfgRd");
    typeFilter->addItem("CfgWr");
    typeFilter->addItem("MemRd");
    typeFilter->addItem("MemWr");
    typeFilter->addItem("Cpl");

    directionFilter->addItem("All directions");
    directionFilter->addItem("TX");
    directionFilter->addItem("RX");

    backendFilter->addItem("pci");
    backendFilter->addItem("dummy");
    backendFilter->addItem("fpga");
    backendFilter->addItem("armds");
    backendFilter->addItem("xgig");

    deviceIdBox->setPlaceholderText("PCI device (default: 0000:00:03.0)");
    deviceIdBox->setText("0000:00:03.0");
    searchBox->setPlaceholderText("Filter by requester ID or address");

    toolbar->addWidget(openButton);
    toolbar->addWidget(enumerateButton);
    toolbar->addWidget(themeButton);
    toolbar->addWidget(new QLabel("Backend:", this));
    toolbar->addWidget(backendFilter);
    toolbar->addWidget(new QLabel("Device:", this));
    toolbar->addWidget(deviceIdBox);
    toolbar->addWidget(new QLabel("Type:", this));
    toolbar->addWidget(typeFilter);
    toolbar->addWidget(new QLabel("Direction:", this));
    toolbar->addWidget(directionFilter);
    toolbar->addWidget(searchBox);

    QWidget *statsWidget = new QWidget(this);
    QHBoxLayout *statsLayout = new QHBoxLayout(statsWidget);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(12);

    totalLabel->setFrameShape(QFrame::StyledPanel);
    txLabel->setFrameShape(QFrame::StyledPanel);
    rxLabel->setFrameShape(QFrame::StyledPanel);
    filteredLabel->setFrameShape(QFrame::StyledPanel);

    totalLabel->setAlignment(Qt::AlignCenter);
    txLabel->setAlignment(Qt::AlignCenter);
    rxLabel->setAlignment(Qt::AlignCenter);
    filteredLabel->setAlignment(Qt::AlignCenter);

    totalLabel->setMinimumWidth(120);
    txLabel->setMinimumWidth(120);
    rxLabel->setMinimumWidth(120);
    filteredLabel->setMinimumWidth(140);

    statsLayout->addWidget(totalLabel);
    statsLayout->addWidget(txLabel);
    statsLayout->addWidget(rxLabel);
    statsLayout->addWidget(filteredLabel);

    tableView = new QTableView(this);
    detailsView = new QTextEdit(this);
    detailsView->setReadOnly(true);
    detailsView->setPlaceholderText("Select a packet to inspect details");
    detailsView->setMinimumHeight(120);

    decodeTree = new QTreeWidget(this);
    decodeTree->setHeaderLabels({"Field", "Value"});
    decodeTree->setColumnWidth(0, 220);
    decodeTree->setAlternatingRowColors(true);
    decodeTree->setMinimumHeight(150);

    QSplitter *detailsSplitter = new QSplitter(Qt::Vertical, this);
    detailsSplitter->addWidget(detailsView);
    detailsSplitter->addWidget(decodeTree);
    detailsSplitter->setStretchFactor(0, 2);
    detailsSplitter->setStretchFactor(1, 1);

    model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({
        "Timestamp",
        "Direction",
        "Type",
        "Requester",
        "Completer",
        "Tag",
        "Length",
        "Addr",
        "Payload"
    });

    tableView->setModel(model);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->setAlternatingRowColors(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setSortingEnabled(false);

    splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(tableView);
    splitter->addWidget(detailsSplitter);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    layout->addLayout(toolbar);
    layout->addWidget(statsWidget);
    layout->addWidget(splitter);
    layout->addWidget(statusLabel);

    setCentralWidget(central);
    setWindowTitle("pcieshark");
    resize(1100, 700);

    applyTheme();

    openButton->setObjectName("openButton");
    themeButton->setObjectName("themeButton");
    statusLabel->setObjectName("statusLabel");
    totalLabel->setObjectName("summaryCard");
    txLabel->setObjectName("summaryCard");
    rxLabel->setObjectName("summaryCard");
    filteredLabel->setObjectName("summaryCard");

    connect(openButton, &QPushButton::clicked, this, &MainWindow::openTrace);
    connect(enumerateButton, &QPushButton::clicked, this, &MainWindow::enumeratePciDevice);
    connect(themeButton, &QPushButton::clicked, this, [this]() {
        darkMode = !darkMode;
        applyTheme();
    });
    connect(typeFilter, &QComboBox::currentTextChanged, this, &MainWindow::applyFilter);
    connect(directionFilter, &QComboBox::currentTextChanged, this, &MainWindow::applyFilter);
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::applyFilter);
    connect(tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::showPacketDetails);
}

void MainWindow::applyTheme()
{
    if (darkMode) {
        setStyleSheet(R"(
            QMainWindow {
                background: #0f172a;
                color: #e2e8f0;
            }
            QWidget {
                font-family: "Segoe UI", "Noto Sans", sans-serif;
                color: #e2e8f0;
            }
            QVBoxLayout, QHBoxLayout {
                spacing: 8px;
            }
            QPushButton {
                background: #1e293b;
                border: 1px solid #334155;
                border-radius: 6px;
                padding: 6px 12px;
                color: #e2e8f0;
                min-height: 28px;
            }
            QPushButton:hover {
                background: #243244;
            }
            QPushButton#openButton {
                background: #2563eb;
                border: 1px solid #2563eb;
                color: #ffffff;
                font-weight: 600;
            }
            QPushButton#themeButton {
                background: #232f3e;
            }
            QComboBox, QLineEdit, QTextEdit {
                background: #111827;
                color: #e2e8f0;
                border: 1px solid #334155;
                border-radius: 6px;
                padding: 6px 10px;
                selection-background-color: #1d4ed8;
                selection-color: #ffffff;
            }
            QTableView {
                background: #0b1220;
                alternate-background-color: #111827;
                color: #e2e8f0;
                border: 1px solid #334155;
                gridline-color: #273548;
                selection-background-color: #1d4ed8;
                selection-color: #ffffff;
                font-size: 12px;
            }
            QTableView::item {
                padding: 4px 6px;
                border: 0px;
                background: #0b1220;
                color: #e2e8f0;
            }
            QTableView::item:alternate {
                background: #111827;
            }
            QTableView::item:selected {
                background: #1d4ed8;
                color: #ffffff;
                border: 1px solid #60a5fa;
            }
            QHeaderView::section {
                background: #172033;
                color: #e2e8f0;
                padding: 8px 6px;
                font-weight: 600;
                border: 1px solid #334155;
            }
            QSplitter::handle {
                background: #334155;
            }
            QTextEdit {
                background: #0f172a;
                border: 1px solid #334155;
                border-radius: 6px;
                padding: 8px;
            }
            QTreeWidget {
                background: #0b1220;
                alternate-background-color: #111827;
                color: #e2e8f0;
                border: 1px solid #334155;
                border-radius: 6px;
                selection-background-color: #1d4ed8;
                selection-color: #ffffff;
            }
            QTreeWidget::item {
                color: #e2e8f0;
                background: transparent;
                border: none;
                padding: 4px 2px;
            }
            QTreeWidget::item:selected {
                background: #1d4ed8;
                color: #ffffff;
                border: 1px solid #60a5fa;
            }
            QLabel#statusLabel {
                background: transparent;
                color: #94a3b8;
                padding-top: 4px;
                font-size: 12px;
            }
            QLabel#summaryCard {
                background: #111827;
                border: 1px solid #334155;
                border-radius: 6px;
                padding: 8px 10px;
                min-height: 38px;
                font-weight: 600;
                color: #e2e8f0;
            }
        )");
        themeButton->setText("Light");
    } else {
        setStyleSheet(R"(
            QMainWindow {
                background: #f3f4f6;
                color: #1f2328;
            }
            QWidget {
                font-family: "Segoe UI", "Noto Sans", sans-serif;
                color: #1f2328;
            }
            QVBoxLayout, QHBoxLayout {
                spacing: 8px;
            }
            QPushButton {
                background: #ffffff;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 6px 12px;
                color: #1f2328;
                min-height: 28px;
            }
            QPushButton:hover {
                background: #f6f8fa;
                border-color: #b6c2cf;
            }
            QPushButton:pressed {
                background: #eaeef2;
            }
            QPushButton#openButton {
                background: #2f6feb;
                border: 1px solid #2f6feb;
                color: #ffffff;
                font-weight: 600;
            }
            QPushButton#themeButton {
                background: #ffffff;
            }
            QComboBox, QLineEdit, QTextEdit {
                background: #ffffff;
                color: #1f2328;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 6px 10px;
                selection-background-color: #cfe2ff;
                selection-color: #1f2328;
            }
            QTableView {
                background: #fbfbfc;
                alternate-background-color: #f2f5f8;
                color: #1f2328;
                border: 1px solid #d0d7de;
                gridline-color: #dfe3e8;
                selection-background-color: #dfeaff;
                selection-color: #1f2328;
                font-size: 12px;
            }
            QTableView::item {
                padding: 4px 6px;
                border: 0px;
                background: #fbfbfc;
                color: #1f2328;
            }
            QTableView::item:alternate {
                background: #f2f5f8;
            }
            QTableView::item:selected {
                background: #dfeaff;
                color: #1f2328;
                border: 1px solid #93c5fd;
            }
            QHeaderView::section {
                background: #e9edf3;
                color: #1f2328;
                padding: 8px 6px;
                font-weight: 600;
                border: 1px solid #d0d7de;
            }
            QLabel {
                color: #1f2328;
            }
            QSplitter::handle {
                background: #d0d7de;
            }
            QTextEdit {
                background: #f8fafc;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 8px;
            }
            QTreeWidget {
                background: #f8fafc;
                alternate-background-color: #f1f5f9;
                color: #1f2328;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                selection-background-color: #dfeaff;
                selection-color: #1f2328;
            }
            QTreeWidget::item {
                color: #1f2328;
                background: transparent;
                border: none;
                padding: 4px 2px;
            }
            QTreeWidget::item:selected {
                background: #dfeaff;
                color: #1f2328;
            }
            QLabel#statusLabel {
                background: transparent;
                color: #4b5563;
                padding-top: 4px;
                font-size: 12px;
            }
            QLabel#summaryCard {
                background: #ffffff;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 8px 10px;
                min-height: 38px;
                font-weight: 600;
                color: #1f2328;
            }
        )");
        themeButton->setText("Dark");
    }
}

void MainWindow::loadTraceFile(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    loadCsv(path);
}

void MainWindow::openTraceDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open PCIe trace",
        QString(),
        "CSV files (*.csv);;All files (*.*)");

    if (path.isEmpty()) {
        return;
    }

    loadTraceFile(path);
}

void MainWindow::openTrace()
{
    openTraceDialog();
}

QStringList MainWindow::splitCsvLine(const QString &line) const
{
    QStringList result;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == '"') {
                current += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            result << current;
            current.clear();
        } else {
            current += ch;
        }
    }

    result << current;
    return result;
}

void MainWindow::loadCsv(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Open failed", "Unable to open trace file: " + path);
        return;
    }

    model->removeRows(0, model->rowCount());
    QTextStream stream(&file);
    QString headerLine = stream.readLine();
    if (headerLine.isEmpty()) {
        file.close();
        QMessageBox::warning(this, "Invalid trace", "The trace file is empty.");
        return;
    }

    int row = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        const auto fields = splitCsvLine(line);
        if (fields.size() < 9) {
            continue;
        }

        const QString ts = fields.at(0).trimmed();
        const QString dir = normalizeDirection(fields.at(1));
        const QString type = sanitizeType(fields.at(2));
        const QString requester = fields.at(3).trimmed();
        const QString completer = fields.at(4).trimmed();
        const QString tag = fields.at(5).trimmed();
        const QString length = fields.at(6).trimmed();
        const QString addr = fields.at(7).trimmed();
        const QString payload = fields.at(8).trimmed();

        const QList<QStandardItem *> items = {
            new QStandardItem(ts),
            new QStandardItem(dir),
            new QStandardItem(type),
            new QStandardItem(requester),
            new QStandardItem(completer),
            new QStandardItem(tag),
            new QStandardItem(length),
            new QStandardItem(addr),
            new QStandardItem(payload)
        };

        model->insertRow(row, items);
        ++row;
    }

    file.close();
    applyFilter();
    if (model->rowCount() > 0) {
        tableView->selectRow(0);
    }
    updateSummaryStats();
    statusLabel->setText(QString("Loaded %1 TLP entries from %2").arg(row).arg(path));
}

void MainWindow::enumeratePciDevice()
{
    QString backend = backendFilter->currentText();
    const QString deviceId = deviceIdBox->text().trimmed();

    if (!deviceId.isEmpty() && backend == "pci") {
        setenv("PCIE_PCI_DEVICE", deviceId.toLocal8Bit().constData(), 1);
    } else {
        unsetenv("PCIE_PCI_DEVICE");
    }

    pcie_ctx_t *ctx = pcie_open(backend.toLocal8Bit().constData());
    if (!ctx) {
        QMessageBox::warning(this, "PCI enumeration failed",
                             "Unable to open the selected backend. Make sure the backend exists and the device is accessible.");
        return;
    }

    pcie_device_info_t deviceInfo = {};
    pcie_link_status_t linkStatus = {};
    int rc = pcie_get_device_info(ctx, &deviceInfo);
    if (rc != 0) {
        QMessageBox::warning(this, "PCI enumeration failed",
                             "The PCI backend is available but the device could not be queried.");
        pcie_close(ctx);
        return;
    }

    pcie_get_link_status(ctx, &linkStatus);

    model->removeRows(0, model->rowCount());

    static const uint64_t enumAddrs[] = {0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c};
    for (size_t i = 0; i < sizeof(enumAddrs) / sizeof(enumAddrs[0]); ++i) {
        pcie_tlp_t cfgRead = pcie_tlp_cfg_read(static_cast<uint32_t>(enumAddrs[i]));
        uint8_t payload[4] = {0};
        cfgRead.requester_id = 0x0001;
        cfgRead.tag = 0x0F;
        cfgRead.length = 4;
        cfgRead.mem.data = payload;

        const int result = pcie_send(ctx, &cfgRead);
        if (result != 0) {
            statusLabel->setText(QString("PCI enumeration failed at offset 0x%1").arg(enumAddrs[i], 0, 16));
            pcie_close(ctx);
            return;
        }

        QString payloadHex;
        for (int j = 0; j < cfgRead.length; ++j) {
            payloadHex += QString("%1").arg(payload[j], 2, 16, QLatin1Char('0'));
            if (j + 1 < cfgRead.length) {
                payloadHex += " ";
            }
        }

        const QString ts = QString::number(QDateTime::currentMSecsSinceEpoch());
        const QList<QStandardItem *> items = {
            new QStandardItem(ts),
            new QStandardItem("TX"),
            new QStandardItem("CfgRd"),
            new QStandardItem("1"),
            new QStandardItem("0"),
            new QStandardItem("15"),
            new QStandardItem("4"),
            new QStandardItem(QString("0x%1").arg(enumAddrs[i], 0, 16)),
            new QStandardItem(payloadHex)
        };

        model->insertRow(static_cast<int>(i), items);
    }

    pcie_close(ctx);

    applyFilter();
    if (model->rowCount() > 0) {
        tableView->selectRow(0);
    }
    updateSummaryStats();

    const QString linkSpeedName = (linkStatus.negotiated_link_speed == PCIE_LINK_SPEED_UNKNOWN)
        ? QStringLiteral("Unknown")
        : QString::fromUtf8(pcie_link_speed_name(linkStatus.negotiated_link_speed));

    const QString deviceSummary = QString("Vendor 0x%1 Device 0x%2 Class 0x%3 Rev 0x%4 | Link %5/%6 lanes")
        .arg(deviceInfo.vendor_id, 4, 16, QLatin1Char('0'))
        .arg(deviceInfo.device_id, 4, 16, QLatin1Char('0'))
        .arg(deviceInfo.class_code, 6, 16, QLatin1Char('0'))
        .arg(deviceInfo.revision_id, 2, 16, QLatin1Char('0'))
        .arg(linkSpeedName)
        .arg(linkStatus.negotiated_link_width);

    statusLabel->setText(QString("Enumerated PCI device (%1)").arg(deviceSummary));
}

void MainWindow::applyFilter()
{
    const QString type = typeFilter->currentText();
    const QString direction = directionFilter->currentText();
    const QString text = searchBox->text().trimmed();

    for (int row = 0; row < model->rowCount(); ++row) {
        bool visible = true;
        const QString rowType = model->index(row, 2).data().toString();
        const QString rowDirection = model->index(row, 1).data().toString();
        const QString rowText = (rowType + " " +
                                 model->index(row, 3).data().toString() + " " +
                                 model->index(row, 7).data().toString()).toLower();

        if (type != "All types" && rowType != type) {
            visible = false;
        }

        if (direction != "All directions" && rowDirection != direction) {
            visible = false;
        }

        if (!text.isEmpty() && !rowText.contains(text.toLower())) {
            visible = false;
        }

        tableView->setRowHidden(row, !visible);
    }

    colorRows();
    showPacketDetails();
    updateSummaryStats();
}

void MainWindow::updateSummaryStats()
{
    int total = 0;
    int tx = 0;
    int rx = 0;
    int visible = 0;

    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 1).data().toString() == "TX") {
            tx++;
        } else if (model->index(row, 1).data().toString() == "RX") {
            rx++;
        }

        total++;
        if (!tableView->isRowHidden(row)) {
            visible++;
        }
    }

    totalLabel->setText(QString("Total: %1").arg(total));
    txLabel->setText(QString("TX: %1").arg(tx));
    rxLabel->setText(QString("RX: %1").arg(rx));
    filteredLabel->setText(QString("Visible: %1").arg(visible));
}

void MainWindow::colorRows()
{
    const bool hasSelection = tableView->currentIndex().isValid();

    for (int row = 0; row < model->rowCount(); ++row) {
        const QString direction = model->index(row, 1).data().toString();
        const QString type = model->index(row, 2).data().toString();
        const bool isSelected = hasSelection && tableView->currentIndex().row() == row;

        QColor bgColor = darkMode ? QColor("#17263a") : QColor("#f4f6fb");
        QColor fgColor = darkMode ? QColor("#e5edf7") : QColor("#1f2328");

        if (isSelected) {
            bgColor = darkMode ? QColor("#2a68bf") : QColor("#dfeaff");
            fgColor = darkMode ? QColor("#ffffff") : QColor("#0f172a");
        } else if (darkMode) {
            bgColor = QColor("#1a2d3d");

            if (direction == "TX") {
                bgColor = QColor("#1d3a4f");
            } else if (direction == "RX") {
                bgColor = QColor("#1f382e");
            }

            if (type == "CfgRd") {
                bgColor = QColor("#224b70");
            } else if (type == "CfgWr") {
                bgColor = QColor("#5a3240");
            } else if (type == "MemRd") {
                bgColor = QColor("#1e3d5a");
            } else if (type == "MemWr") {
                bgColor = QColor("#594d25");
            } else if (type == "Cpl") {
                bgColor = QColor("#2e3459");
            }
        } else {
            if (direction == "TX") {
                bgColor = QColor("#eaf4ff");
            } else if (direction == "RX") {
                bgColor = QColor("#edf9ee");
            }

            if (type == "CfgRd") {
                bgColor = QColor("#eaf1ff");
            } else if (type == "CfgWr") {
                bgColor = QColor("#fff0ef");
            } else if (type == "MemRd") {
                bgColor = QColor("#eef3ff");
            } else if (type == "MemWr") {
                bgColor = QColor("#fff8eb");
            } else if (type == "Cpl") {
                bgColor = QColor("#f2f0ff");
            }
        }

        for (int col = 0; col < model->columnCount(); ++col) {
            auto *item = model->item(row, col);
            if (!item) {
                continue;
            }
            item->setBackground(bgColor);
            item->setForeground(fgColor);
        }
    }
}

void MainWindow::showPacketDetails()
{
    const QModelIndex index = tableView->currentIndex();
    if (!index.isValid()) {
        detailsView->setPlainText("No packet selected.");
        decodeTree->clear();
        return;
    }

    const int row = index.row();
    const QString timestamp = model->index(row, 0).data().toString();
    const QString direction = model->index(row, 1).data().toString();
    const QString typeText = model->index(row, 2).data().toString();
    const QString requester = model->index(row, 3).data().toString();
    const QString completer = model->index(row, 4).data().toString();
    const QString tag = model->index(row, 5).data().toString();
    const QString lengthText = model->index(row, 6).data().toString();
    const QString addrText = model->index(row, 7).data().toString();
    const QString payload = model->index(row, 8).data().toString();

    bool ok = false;
    const uint8_t typeCode = static_cast<uint8_t>(typeText.toUInt(&ok, 16));
    const uint16_t requesterId = static_cast<uint16_t>(requester.toUInt());
    const uint16_t completerId = static_cast<uint16_t>(completer.toUInt());
    const uint8_t tagValue = static_cast<uint8_t>(tag.toUInt());
    const uint16_t lengthValue = static_cast<uint16_t>(lengthText.toUInt());
    const uint64_t addrValue = static_cast<uint64_t>(addrText.toULongLong(nullptr, 16));
    const pcie_tlp_type_t decodedType = pcie_tlp_type_from_code(typeCode);
    const pcie_tlp_t decoded = pcie_tlp_decode(typeCode, requesterId, completerId,
                                              tagValue, lengthValue, addrValue, nullptr);

    const QString typeName = pcie_tlp_type_name(decodedType);
    const QString payloadHex = formatHexDump(payload);

    QString details;
    details += QString("Packet #%1\n").arg(row + 1);
    details += QString("Timestamp: %1\n").arg(timestamp);
    details += QString("Direction: %1\n").arg(direction);
    details += QString("Type: %1 (0x%2)\n\n").arg(typeName).arg(typeText);
    details += QString("Header fields:\n");
    details += QString("  - type: %1\n").arg(typeName);
    details += QString("  - requester_id: %1\n").arg(decoded.requester_id);
    details += QString("  - completer_id: %1\n").arg(decoded.completer_id);
    details += QString("  - tag: %1\n").arg(decoded.tag);
    details += QString("  - length: %1\n").arg(decoded.length);
    details += QString("  - address: 0x%1\n").arg(decoded.mem.addr, 0, 16);
    details += QString("  - payload_len: %1\n\n").arg(payload.isEmpty() ? 0 : payload.size());
    details += QString("Payload (hex):\n%1\n").arg(payloadHex);
    details += QString("Raw payload string: %1\n").arg(payload.isEmpty() ? QString("<none>") : payload);

    detailsView->setPlainText(details);

    decodeTree->clear();
    auto *root = new QTreeWidgetItem(decodeTree, {"PCIe TLP", ""});
    auto addField = [&](const QString &name, const QString &value) {
        new QTreeWidgetItem(root, {name, value});
    };

    addField("Direction", direction);
    addField("Type", QString("%1 (0x%2)").arg(typeName).arg(typeText));
    addField("Requester ID", requester);
    addField("Completer ID", completer);
    addField("Tag", tag);
    addField("Length", lengthText);
    addField("Address", QString("0x%1").arg(addrValue, 0, 16));
    addField("Payload length", QString::number(payload.isEmpty() ? 0 : payload.size()));
    addField("Payload", payload.isEmpty() ? "<none>" : payload);

    if ((typeText == "CfgRd" || typeText == "CfgWr") && !payload.isEmpty()) {
        QString normalizedPayload = payload;
        normalizedPayload.remove(' ');
        normalizedPayload.remove('\t');
        normalizedPayload.remove('\n');
        normalizedPayload.remove('\r');

        const QByteArray rawBytes = QByteArray::fromHex(normalizedPayload.toLatin1());
        if (rawBytes.size() >= 4) {
            uint32_t regValue = 0;
            for (int i = 0; i < 4 && i < rawBytes.size(); ++i) {
                regValue |= static_cast<uint32_t>(static_cast<unsigned char>(rawBytes.at(i))) << (8 * i);
            }

            auto *pciRoot = new QTreeWidgetItem(decodeTree, {"PCI config decode", ""});
            const uint64_t regAddr = addrValue;

            switch (static_cast<int>(regAddr)) {
                case 0x00:
                    new QTreeWidgetItem(pciRoot, {"Vendor ID", makeHexLabel(static_cast<uint16_t>(regValue & 0xFFFF), 4)});
                    new QTreeWidgetItem(pciRoot, {"Vendor name", pciVendorName(static_cast<uint16_t>(regValue & 0xFFFF))});
                    new QTreeWidgetItem(pciRoot, {"Device ID", makeHexLabel(static_cast<uint16_t>((regValue >> 16) & 0xFFFF), 4)});
                    break;
                case 0x04:
                    new QTreeWidgetItem(pciRoot, {"Command", makeHexLabel(static_cast<uint16_t>(regValue & 0xFFFF), 4)});
                    new QTreeWidgetItem(pciRoot, {"Status", makeHexLabel(static_cast<uint16_t>((regValue >> 16) & 0xFFFF), 4)});
                    break;
                case 0x08:
                    new QTreeWidgetItem(pciRoot, {"Revision ID", makeHexLabel(static_cast<uint8_t>(regValue & 0xFF), 2)});
                    new QTreeWidgetItem(pciRoot, {"Class code", makeHexLabel(static_cast<uint32_t>((regValue >> 8) & 0xFFFFFF), 6)});
                    new QTreeWidgetItem(pciRoot, {"Class name", pciClassName(static_cast<uint32_t>((regValue >> 8) & 0xFFFFFF))});
                    break;
                case 0x10:
                case 0x14:
                case 0x18:
                case 0x1C:
                    new QTreeWidgetItem(pciRoot, {"BAR", makeHexLabel(regValue, 8)});
                    break;
                default:
                    new QTreeWidgetItem(pciRoot, {"Raw register", makeHexLabel(regValue, 8)});
                    break;
            }

            auto *deviceSummary = new QTreeWidgetItem(decodeTree, {"PCI device summary", ""});
            const uint16_t vendorId = (regAddr == 0x00) ? static_cast<uint16_t>(regValue & 0xFFFF) : 0;
            const uint16_t deviceId = (regAddr == 0x00) ? static_cast<uint16_t>((regValue >> 16) & 0xFFFF) : 0;
            const uint32_t classCode = (regAddr == 0x08) ? static_cast<uint32_t>((regValue >> 8) & 0xFFFFFF) : 0;

            if (vendorId != 0) {
                new QTreeWidgetItem(deviceSummary, {"Vendor", QString("%1 (%2)").arg(pciVendorName(vendorId)).arg(makeHexLabel(vendorId, 4))});
                new QTreeWidgetItem(deviceSummary, {"Device", makeHexLabel(deviceId, 4)});
            }
            if (classCode != 0) {
                new QTreeWidgetItem(deviceSummary, {"Class", QString("%1 (%2)").arg(pciClassName(classCode)).arg(makeHexLabel(classCode, 6))});
            }
            if (regAddr == 0x10 || regAddr == 0x14 || regAddr == 0x18 || regAddr == 0x1C) {
                new QTreeWidgetItem(deviceSummary, {"BAR", makeHexLabel(regValue, 8)});
            }
        }
    }

    auto *specRoot = new QTreeWidgetItem(decodeTree, {"Spec structure", ""});
    new QTreeWidgetItem(specRoot, {"type", typeName});
    new QTreeWidgetItem(specRoot, {"requester_id", QString::number(decoded.requester_id)});
    new QTreeWidgetItem(specRoot, {"completer_id", QString::number(decoded.completer_id)});
    new QTreeWidgetItem(specRoot, {"tag", QString::number(decoded.tag)});
    new QTreeWidgetItem(specRoot, {"length", QString::number(decoded.length)});
    new QTreeWidgetItem(specRoot, {"addr", QString("0x%1").arg(decoded.mem.addr, 0, 16)});
    new QTreeWidgetItem(specRoot, {"payload_len", QString::number(payload.isEmpty() ? 0 : payload.size())});
    decodeTree->expandAll();
}
