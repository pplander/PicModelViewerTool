#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QList>

class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget* parent = nullptr);

    void retranslateUI();

signals:
    void openClicked();
    void recentFileClicked(const QString& filePath);

public slots:
    void setRecentFiles(const QStringList& files);

private:
    void setupUI();

    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QPushButton* m_openButton = nullptr;
    QWidget* m_recentPanel = nullptr;
    QVBoxLayout* m_recentLayout = nullptr;
    QLabel* m_recentTitleLabel = nullptr;
    QLabel* m_tipLabel = nullptr;
};
