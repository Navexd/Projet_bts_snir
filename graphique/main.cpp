#include <QApplication>
#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

bool connectToDatabase() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QMARIADB");
    db.setHostName("10.194.177.90");  // Remplacez par l'adresse IP de votre serveur MariaDB
    db.setDatabaseName("sono");
    db.setUserName("pi");
    db.setPassword("raspi");

    if (!db.open()) {
        qDebug() << "Error: " << db.lastError().text();
        return false;
    }

    return true;
}

QVector<QPair<int, QPointF>> fetchData() {
    QVector<QPair<int, QPointF>> data;
    QSqlQuery query("SELECT id, humidity, noise FROM sono");

    while (query.next()) {
        int id = query.value(0).toInt();
        qreal humidity = query.value(1).toDouble();
        qreal noise = query.value(2).toDouble();
        data.append(qMakePair(id, QPointF(humidity, noise)));
    }

    return data;
}

void displayChart(const QVector<QPair<int, QPointF>>& data) {
    QtCharts::QChart *chart = new QtCharts::QChart();

    QtCharts::QLineSeries *humiditySeries = new QtCharts::QLineSeries();
    QtCharts::QLineSeries *noiseSeries = new QtCharts::QLineSeries();

    for (const auto &entry : data) {
        humiditySeries->append(entry.first, entry.second.x());
        noiseSeries->append(entry.first, entry.second.y());
    }

    chart->addSeries(humiditySeries);
    chart->addSeries(noiseSeries);

    // Ajout d'étiquettes de données pour les séries d'humidité et de bruit
    humiditySeries->setPointLabelsVisible(true);
    humiditySeries->setPointLabelsFormat("@yPoint");

    noiseSeries->setPointLabelsVisible(true);
    noiseSeries->setPointLabelsFormat("@yPoint");

    chart->createDefaultAxes();
    chart->axes(Qt::Vertical).first()->setTitleText("Value");

    // Définition de l'intervalle de l'axe des ordonnées
    chart->axes(Qt::Vertical).first()->setRange(0, 130);

    // Ajout de marges au graphe pour éviter que les valeurs ne soient tronquées
    chart->setMargins(QMargins(50, 50, 50, 50));

    QtCharts::QChartView *chartView = new QtCharts::QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // Capturer une image du graphique
    QImage image = chartView->grab().toImage();

    // Enregistrer l'image dans un fichier
    image.save("chart_image.png");

    // Ajouter un bouton d'impression dans le contenu HTML
    QString htmlContent = "<!DOCTYPE html><html><head><title>Chart</title></head><body>";
    htmlContent += "<img src='chart_image.png'><br>";
    htmlContent += "<button onclick=\"window.print()\">Imprimer</button>";
    htmlContent += "</body></html>";

    // Enregistrer le contenu HTML dans un fichier
    QFile file("chart.html");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << htmlContent;
        file.close();
    }

    // Ouvrir le fichier HTML dans un navigateur web
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo("chart.html").absoluteFilePath()));
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    if (!connectToDatabase()) {
        return -1;
    }

    QVector<QPair<int, QPointF>> data = fetchData();
    displayChart(data);

    return 0;
}
