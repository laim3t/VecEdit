#ifndef PINVALUEEDIT_H
#define PINVALUEEDIT_H

#include <QLineEdit>

// 自定义LineEdit类，用于限制输入和自动转换大写
class PinValueLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    PinValueLineEdit(QWidget *parent = nullptr) : QLineEdit(parent)
    {
        setMaxLength(1);               // 限制输入一个字符
        setAlignment(Qt::AlignCenter); // 文本居中显示
        setToolTip("输入提示：0,1,L,l,H,h,X,x,；默认：X");

        // 设置样式以匹配标准表格单元格
        setStyleSheet("QLineEdit { border: none; }");

        // 设置帧形状为无框架
        setFrame(false);

        // 设置验证器
        connect(this, &QLineEdit::textChanged, this, &PinValueLineEdit::validateAndConvert);
    }

protected slots:
    void validateAndConvert(const QString &text)
    {
        if (text.isEmpty())
            return;

        QString validChars = "01LlHhXx";
        QChar ch = text[0];

        // 检查是否为有效字符
        if (!validChars.contains(ch))
        {
            setText(""); // 无效字符，清空输入
            return;
        }

        // 将小写字母转换为大写
        if (ch == 'l' || ch == 'h' || ch == 'x')
        {
            blockSignals(true); // 阻止再次触发textChanged信号
            setText(text.toUpper());
            blockSignals(false);
        }
    }
};

#endif // PINVALUEEDIT_H