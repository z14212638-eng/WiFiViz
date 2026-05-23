#include "antennas.h"
#include "ui_antennas.h"
#include "config_ui_style.h"
#include <iostream>

Antenna::Antenna(QWidget *parent) : QDialog(parent), ui(new Ui::Antenna) {
  ui->setupUi(this);
  ConfigUiStyle::RebuildAntennaConfig(this);
  centerWindow(this);

  insertDefaultAntenna();
}

Antenna::~Antenna() { delete ui; }

void Antenna::on_pushButton_clicked() {
  ui->widget->show();
  qint8 item_num = ui->tableWidget->rowCount();
  ui->tableWidget->setRowCount(item_num + 1);
  ui->pushButton->setEnabled(false);
  ui->pushButton_2->setEnabled(false);
  ui->buttonBox->setEnabled(false);
}

// Page_antenna_Confirm(adding a new antenna to the list)
void Antenna::on_pushButton_3_clicked() {
  QString antenna_type = ui->comboBox->currentText();
  qint16 MaxGain = ui->spinBox->value();
  qint16 BeamWidth = ui->spinBox_2->value();
  qint8 rowCount = ui->tableWidget->rowCount();
  qint8 lastrow = rowCount - 1;
  std::cout << antenna_type.toStdString() << "  " << MaxGain << " " << BeamWidth
            << " " << rowCount << std::endl;
  ui->tableWidget->setItem(lastrow, 0, new QTableWidgetItem(antenna_type));
  ui->tableWidget->setItem(lastrow, 1,
                           new QTableWidgetItem(QString::number(MaxGain)));
  ui->tableWidget->setItem(lastrow, 2,
                           new QTableWidgetItem(QString::number(BeamWidth)));
  ui->pushButton->setEnabled(true);
  ui->pushButton_2->setEnabled(true);
  ui->buttonBox->setEnabled(true);
  ui->widget->close();
}

void Antenna::on_pushButton_2_clicked() {
  qint8 rowCount = ui->tableWidget->rowCount();
  qint8 lastrow = rowCount - 1;
  ui->tableWidget->removeRow(lastrow);
}

void Antenna::on_buttonBox_accepted() {
  antenna_list.clear();

  int rowCount = ui->tableWidget->rowCount();
  for (int i = 0; i < rowCount; ++i) {
    auto *t = ui->tableWidget->item(i, 0);
    auto *g = ui->tableWidget->item(i, 1);
    auto *b = ui->tableWidget->item(i, 2);

    if (!t || !g || !b)
      continue;

    antenna_list.append(std::make_shared<Antenna_Config>(
        t->text(), g->text().toInt(), b->text().toInt()));
  }

  // 🛡 兜底：用户删光了
  if (antenna_list.isEmpty()) {
    antenna_list.append(
        std::make_shared<Antenna_Config>("Isotropic", 0, 360));
  }

  emit BackToLastPage();
}

qint16 Antenna::AntennaCount() const { return ui->tableWidget->rowCount(); }

void Antenna::resetPage() {
  is_ap = false;
  is_sta = false;

  ui->widget->close();

  ui->comboBox->setCurrentIndex(0);
  ui->spinBox->setValue(ui->spinBox->minimum());
  ui->spinBox_2->setValue(ui->spinBox_2->minimum());

  ui->pushButton->setEnabled(true);
  ui->pushButton_2->setEnabled(true);
  ui->buttonBox->setEnabled(true);

  insertDefaultAntenna();

  qDebug() << "[Antenna] resetPage() done";
}

void Antenna::insertDefaultAntenna() {
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(1);

    ui->tableWidget->setItem(0, 0, new QTableWidgetItem("Isotropic"));
    ui->tableWidget->setItem(0, 1, new QTableWidgetItem("0"));
    ui->tableWidget->setItem(0, 2, new QTableWidgetItem("360"));

    antenna_list.clear();
    antenna_list.append(
        std::make_shared<Antenna_Config>("Isotropic", 0, 360)
    );
}
