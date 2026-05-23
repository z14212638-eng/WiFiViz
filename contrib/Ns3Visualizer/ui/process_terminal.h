#pragma once

#include <QPlainTextEdit>
#include <QPointer>
#include <QStringList>

class QProcess;

class ProcessTerminalWidget : public QPlainTextEdit
{
    Q_OBJECT
  public:
    explicit ProcessTerminalWidget(QWidget* parent = nullptr);

    void attachProcess(QProcess* process);
    void appendOutput(const QString& text);
    void resetTerminal();

  protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

  private:
    void ensureCursorInEditableArea();
    void showPromptIfNeeded();
    QString currentInput() const;

    QPointer<QProcess> m_process;
    int m_inputStart = 0;
    QStringList m_history;
    int m_historyIndex = -1;
    bool m_promptVisible = false;
};
