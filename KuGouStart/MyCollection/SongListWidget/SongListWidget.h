//
// Created by WeiWang on 24-11-15.
//

#ifndef SONGLISTWIDGET_H
#define SONGLISTWIDGET_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class SongListWidget; }
QT_END_NAMESPACE

class SongListWidget : public QWidget {
Q_OBJECT

public:
    explicit SongListWidget(QWidget *parent = nullptr);
    ~SongListWidget() override;

private:
    Ui::SongListWidget *ui;
};


#endif //SONGLISTWIDGET_H
