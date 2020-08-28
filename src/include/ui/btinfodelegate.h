/**
 * @copyright 2020-2020 Uniontech Technology Co., Ltd.
 *
 * @file btinfodelegate.h
 *
 * @brief bt窗口中表格item代理类
 *
 * @date 2020-06-09 10:49
 *
 * Author: bulongwei  <bulongwei@uniontech.com>
 *
 * Maintainer: bulongwei  <bulongwei@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BTINFODELEGATE_H
#define BTINFODELEGATE_H

#include <DDialog>
#include <QStyledItemDelegate>

class QEvent;
class QAbstractItemModel;
class QStyleOptionViewItem;
class QModelIndex;
class QCheckBox;
class QDir;

DWIDGET_USE_NAMESPACE

class BtInfoDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    BtInfoDelegate(DDialog *dialog);
    ~BtInfoDelegate();

    /**
     * @brief 设置选择行颜色
     * @param c 颜色
     */
    void setHoverColor(QBrush c);

protected:
    /**
     * @brief 重写绘画事件
     * @param painter 画笔
     * @param option 视图小部件中绘制项的参数
     * @param index 数据模型中的数据
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const; //绘制事件
    /**
     * @brief 响应按钮事件 - 划过、按下
     * @param event 事件
     * @param model 模型
     * @param option 视图小部件中绘制项的参数
     * @param index 数据模型中的数据
     */
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index); // 响应按钮事件 - 划过、按下

    //void sort()
private:
    DDialog *m_dialog; //父类窗口指针
    QCheckBox *m_checkBtn;
    int m_hoverRow; //当前选择行
    QBrush m_hoverColor = QColor(0, 0, 0, 13);

public slots:
    /**
     * @brief 选择行变动槽函数
     * @param index 当前选择行
     */
    void onhoverChanged(const QModelIndex &index);
};

#endif // BTINFODELEGATE_H
