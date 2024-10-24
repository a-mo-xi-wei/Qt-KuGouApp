//
// Created by WeiWang on 24-10-10.
//

// You may need to build the project (run Qt uic code generator) to get "ui_LocalDownload.h" resolved

#include "LocalDownload.h"
#include "ui_LocalDownload.h"


#include<QStandardPaths>
#include<QFileDialog>
#include<QMediaMetaData>
#include<QMediaPlayer>
#include<QStringList>
#include<QRandomGenerator>
#include<QRegularExpression>
#include <memory>
#include <QScreen>

// 创建一个宏来截取 __FILE__ 宏中的目录部分
#define GET_CURRENT_DIR (QString(__FILE__).first(qMax(QString(__FILE__).lastIndexOf('/'), QString(__FILE__).lastIndexOf('\\'))))
//匹配是否有乱码
static QRegularExpression re(QStringLiteral("^[A-Za-z0-9\\p{Han}\\\\/\\-_\\*]+$"));

LocalDownload::LocalDownload(QWidget *parent)
    :QWidget(parent)
    ,ui(new Ui::LocalDownload)
    , m_player(std::make_unique<QMediaPlayer>(this))
    ,m_searchAction(new QAction(this))
    ,m_sortOptMenu(new MyMenu(MyMenu::MenuKind::SortOptionMenu,this))
{
    ui->setupUi(this);
    {
        QFile file(GET_CURRENT_DIR + QStringLiteral("/local.css"));
        if (file.open(QIODevice::ReadOnly)) {
            this->setStyleSheet(file.readAll());
        }
        else {
            qDebug() << "样式表打开失败QAQ";
            return;
        }
    }
    getMetaData();
    init();
    connect(m_sortOptMenu,&MyMenu::selected,this,[this] {
        //ui->local_sort_toolButton->setStyleSheet("border-image:url('://Res/titlebar/sort-blue.svg');");
        ui->local_sort_toolButton->setStyleSheet("QToolButton{border-image:url('://Res/titlebar/sort-blue.svg');}");
    });
    connect(m_sortOptMenu,&MyMenu::deselected,this,[this] {
        //qDebug()<<"接收到无排序状态";
        ui->local_sort_toolButton->setStyleSheet(R"(
                QToolButton{border-image:url('://Res/titlebar/sort-gray.svg');}
                QToolButton:hover{border-image:url('://Res/titlebar/sort-blue.svg');})");
    });
    connect(m_sortOptMenu,&MyMenu::defaultSort,this,&LocalDownload::onDefaultSort);
    connect(m_sortOptMenu,&MyMenu::addTimeSort,this,&LocalDownload::onAddTimeSort);
    connect(m_sortOptMenu,&MyMenu::songNameSort,this,&LocalDownload::onSongNameSort);
    connect(m_sortOptMenu,&MyMenu::singerSort,this,&LocalDownload::onSingerSort);
    connect(m_sortOptMenu,&MyMenu::durationSort,this,&LocalDownload::onDurationSort);
    connect(m_sortOptMenu,&MyMenu::playCountSort,this,&LocalDownload::onPlayCountSort);
    connect(m_sortOptMenu,&MyMenu::randomSort,this,&LocalDownload::onRandomSort);

}

LocalDownload::~LocalDownload() {
    delete ui;
}

void LocalDownload::init() {
    auto layout = ui->local_song_list_widget->layout();
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);

    //下标图片
    ui->idx1_lab->setPixmap(QPixmap(QStringLiteral("://Res/window/index_lab.svg")));
    ui->idx2_lab->setPixmap(QPixmap(QStringLiteral("://Res/window/index_lab.svg")));
    ui->idx3_lab->setPixmap(QPixmap(QStringLiteral("://Res/window/index_lab.svg")));
    ui->idx4_lab->setPixmap(QPixmap(QStringLiteral("://Res/window/index_lab.svg")));
    ui->idx2_lab->hide();
    ui->idx3_lab->hide();
    ui->idx4_lab->hide();

    ui->local_play_toolButton->setIcon(QIcon(QStringLiteral("://Res/tabIcon/play3-white.svg")));
    ui->local_add_toolButton->setIcon(QIcon(QStringLiteral("://Res/tabIcon/add-gray.svg")));
    ui->upload_toolButton->setIcon(QIcon(QStringLiteral("://Res/tabIcon/upload-cloud-gray.svg")));

    //使用 addAction 添加右侧图标
    this->m_searchAction->setIcon(QIcon(QStringLiteral("://Res/titlebar/search-black.svg")));
    this->m_searchAction->setIconVisibleInMenu(false);  // 仅显示图标
    ui->local_search_lineEdit->addAction(this->m_searchAction,QLineEdit::TrailingPosition);
    ui->local_search_lineEdit->setWidth(150);

    //先直接往里面嵌入一首歌
    //this->m_songQueue.enqueue();
    this->m_mediaPath = QStringLiteral("qrc:/Res/audio/紫荆花盛开.mp3");
    this->m_player->setSource(QUrl(this->m_mediaPath));
    this->m_player->play();  // 触发状态改变信号，获取元数据信息
}

void LocalDownload::getMetaData() {
    //经过一段时间的搜索得到的结论，如果想不调用play()就获得元数据信息是不现实的，能做到的也就只有加载完成之后立马停止
    connect(m_player.get(), &QMediaPlayer::mediaStatusChanged, [=](const QMediaPlayer::MediaStatus& status) {
        if (status == QMediaPlayer::LoadedMedia) {
            //qDebug()<<"媒体状态改变，加载完成";
            //qDebug()<<"元数据加载完成";
            const QMediaMetaData data = this->m_player->metaData();
            for(auto val : data.keys()) {
                //qDebug()<<val<<": "<<data.value(val).toString();
            }
            // 停止播放
            this->m_player->stop();
            //获取标题
            auto title = data.value(QMediaMetaData::Title).toString();
            if(!re.match(title).hasMatch()) {
                title = QUrl::fromLocalFile(this->m_mediaPath).fileName();
                title = title.first(title.lastIndexOf('.'));
            }
            //获取歌手
            auto singer = data.value(QMediaMetaData::ContributingArtist).toString();
            if(!re.match(singer).hasMatch())singer = QStringLiteral("网络歌手");
            //获取封面
            auto cover = data.value(QMediaMetaData::ThumbnailImage).value<QPixmap>();
            if(cover.isNull()) {
                //qDebug()<<"封面为空";
                cover = QPixmap(QString("://Res/tablisticon/pix%1.png").arg(QRandomGenerator::global()->bounded(1,11)));
            }
            //获取时长
            const auto duration = data.value(QMediaMetaData::Duration).value<qint64>();
            //信息赋值
            SongInfor tempInformation;
            tempInformation.index = this->m_locationMusicVector.size()+1;//让他先加1
            tempInformation.cover = cover;
            tempInformation.songName = title;
            tempInformation.singer = singer;
            tempInformation.duration = QTime::fromMSecsSinceStartOfDay(duration).toString("mm:ss");
            tempInformation.mediaPath = this->m_mediaPath;
            tempInformation.addTime = QDateTime::currentDateTime();
            tempInformation.playCount = 0;
            //判重（通过元数据信息）
            auto it = std::find(this->m_locationMusicVector.begin(),
            this->m_locationMusicVector.end(),tempInformation);
            if(it == this->m_locationMusicVector.end())this->m_locationMusicVector.emplace_back(tempInformation);
            else {
                //qDebug()<<title<<"已存在，请勿重复插入";
                //加载下一首歌
                loadNextSong();
                return;
            }
            //向parent发送添加MediaPath的信号
            emit addSongInfo(tempInformation);
            //加载相关信息
            auto item = new MusicItemWidget(tempInformation, this);
            item->setFillColor(QColor(QStringLiteral("#B0EDF6")));
            item->setRadius(12);
            item->setInterval(1);
            auto index = tempInformation.index;// 捕获当前的 index
            connect(item, &MusicItemWidget::playRequest, this, [index, this] {
                emit playMusic(index);
            });
            //插入Item
            this->m_MusicItemVector.emplace_back(item);
            dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, item);
            ui->local_music_number_label->setText(QString::number(this->m_locationMusicVector.size()));
            //加载下一首歌
            loadNextSong();
        }
    });
}

void LocalDownload::loadNextSong() {
    if (!m_songQueue.isEmpty()) {
        this->m_mediaPath = m_songQueue.dequeue();  // 取出队列中的下一首歌路径
        //qDebug()<<"取出歌曲 ： "<<this->m_mediaPath<<"=================";

        /*// 在加载新媒体前，重置媒体状态
        this->m_player->stop();
        this->m_player->setSource(QUrl());  // 清空当前媒体源
        this->m_player->play();*/ //不起效果

        // 释放并重建 QMediaPlayer 对象
        this->m_player = std::make_unique<QMediaPlayer>(this);//它会自动释放之前的对象
        getMetaData();

        this->m_player->setSource(QUrl::fromLocalFile(this->m_mediaPath));
        this->m_player->play();  // 触发状态改变信号，获取元数据信息
    }
}

void LocalDownload::getMenuPosition(const QPoint &pos) {
    this->m_menuPosition = pos;
    // 获取屏幕的尺寸
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();

    // 计算菜单右侧的全局位置
    //int menuLeftPos = pos.x() - m_menu->width();
    int menuRightPos  = pos.x() + m_sortOptMenu->width();
    int menuBottomPos = pos.y() + m_sortOptMenu->height();
    //int menuTopPos = pos.y() - m_menu->height();
    // 若菜单左侧超出屏幕左侧 (不存在)
    //if(menuLeftPos < 0) {
    //    // 动态调整菜单位置，使其在屏幕内显示
    //    m_menuPosition.setX(10);
    //}
    // 如果菜单右侧超出屏幕右侧
    if (menuRightPos > screenGeometry.right()) {
        // 动态调整菜单位置，使其在屏幕内显示
        int offset = menuRightPos - screenGeometry.right() + 5;
        m_menuPosition.setX(pos.x() - offset);
    }
    // 如果菜单下侧超出屏幕下侧
    if (menuBottomPos > screenGeometry.bottom()) {
        // 动态调整菜单位置，使其在屏幕内显示
        int offset = menuBottomPos - screenGeometry.bottom() + 5;
        m_menuPosition.setY(pos.y() - offset);
    }
    // 如果菜单下侧超出屏幕下侧（不存在）
    //if(menuTopPos < 0) {
    //    // 动态调整菜单位置，使其在屏幕内显示
    //    m_menuPosition.setY(10);
    //}

}

void LocalDownload::on_local_play_toolButton_clicked() {
    emit startPlay();
}

void LocalDownload::on_local_add_toolButton_clicked() {
    QString musicPath = QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first();
    QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("添加音乐"), musicPath, "Music (*.mp3 *.aac *.wav)");
    if (paths.isEmpty())return;
    //QString fileName = QUrl::fromLocalFile(path).fileName();
    //qDebug() << "插入："<<paths.size()<<"条数据";
    for(auto& path : paths) {
        //qDebug()<<"添加歌曲 ："<<path;
        this->m_songQueue.enqueue(path);
    }
    this->loadNextSong();
}

void LocalDownload::on_local_music_pushButton_clicked() {
    ui->idx1_lab->show();
    ui->idx2_lab->hide();
    ui->idx3_lab->hide();
    ui->idx4_lab->hide();
    ui->local_music_number_label->setStyleSheet(QStringLiteral("color:#26a1ff;font-size:16px;font-weight:bold;"));
    ui->downloaded_music_number_label->setStyleSheet("");
    ui->downloaded_video_number_label->setStyleSheet("");
    ui->downloading_number_label->setStyleSheet("");
}

void LocalDownload::on_downloaded_music_pushButton_clicked() {
    ui->idx1_lab->hide();
    ui->idx2_lab->show();
    ui->idx3_lab->hide();
    ui->idx4_lab->hide();
    ui->downloaded_music_number_label->setStyleSheet(QStringLiteral("color:#26a1ff;font-size:16px;font-weight:bold;"));
    ui->local_music_number_label->setStyleSheet("");
    ui->downloaded_video_number_label->setStyleSheet("");
    ui->downloading_number_label->setStyleSheet("");
}

void LocalDownload::on_downloaded_video_pushButton_clicked() {
    ui->idx1_lab->hide();
    ui->idx2_lab->hide();
    ui->idx3_lab->show();
    ui->idx4_lab->hide();
    ui->downloaded_video_number_label->setStyleSheet(QStringLiteral("color:#26a1ff;font-size:16px;font-weight:bold;"));
    ui->downloaded_music_number_label->setStyleSheet("");
    ui->local_music_number_label->setStyleSheet("");
    ui->downloading_number_label->setStyleSheet("");
}

void LocalDownload::on_downloading_pushButton_clicked() {
    ui->idx1_lab->hide();
    ui->idx2_lab->hide();
    ui->idx3_lab->hide();
    ui->idx4_lab->show();
    ui->downloading_number_label->setStyleSheet(QStringLiteral("color:#26a1ff;font-size:16px;font-weight:bold;"));
    ui->downloaded_music_number_label->setStyleSheet("");
    ui->downloaded_video_number_label->setStyleSheet("");
    ui->local_music_number_label->setStyleSheet("");
}

void LocalDownload::setPlayIndex(const int &index) {
    this->m_setPlayIndex = index;
    //if(this->m_curPlatIndex == -1) {
    //    this->m_curPlatIndex = index;
    //    auto widget = dynamic_cast<MusicItemWidget*>(ui->local_song_list_widget->layout()->itemAt(index)->widget());
    //    //增加播放次数
    //    widget->m_information.playCount++;
    //    qDebug()<<"第 "<<index<<"首歌增加次数至："<<widget->m_information.playCount;
    //    widget->setPlayState(true);
    //}
    //else {
    //    auto widget = dynamic_cast<MusicItemWidget*>(ui->local_song_list_widget->layout()->itemAt(this->m_setPlayIndex)->widget());//为了好增加次数
    //    //增加播放次数
    //    widget->m_information.playCount++;
    //    qDebug()<<"第 "<<index<<"首歌增加次数至："<<widget->m_information.playCount;
    //    if(this->m_setPlayIndex != this->m_curPlatIndex) {
    //        widget->setPlayState(true);
    //        widget = dynamic_cast<MusicItemWidget*>(ui->local_song_list_widget->layout()->itemAt(this->m_curPlatIndex)->widget());
    //        widget->setPlayState(false);
    //        this->m_curPlatIndex = this->m_setPlayIndex;
    //    }
    //}
    if(this->m_curPlatIndex == -1) {
        this->m_curPlatIndex = index;
        auto widget = m_MusicItemVector[index];
        //增加播放次数
        widget->m_information.playCount++;
        qDebug()<<"第 "<<index<<"首歌增加次数至："<<widget->m_information.playCount;
        widget->setPlayState(true);
    }
    else {
        auto widget = m_MusicItemVector[this->m_setPlayIndex];//为了好增加次数
        //增加播放次数
        widget->m_information.playCount++;
        qDebug()<<"第 "<<index<<"首歌增加次数至："<<widget->m_information.playCount;
        if(this->m_setPlayIndex != this->m_curPlatIndex) {
            widget->setPlayState(true);
            widget = m_MusicItemVector[this->m_curPlatIndex];
            widget->setPlayState(false);
            this->m_curPlatIndex = this->m_setPlayIndex;
        }
    }

}

void LocalDownload::onDefaultSort() {
    ui->local_sort_toolButton->setToolTip("当前排序方式：默认排序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        //if(item->widget())item->widget()->deleteLater();//不得删除他指向的内容
        delete item;
    }
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则
    auto defaultSort = [](const MusicItemWidget* item1,const MusicItemWidget* item2) {
        return item1->m_information.addTime > item2->m_information.addTime;
    };
    std::sort(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(),defaultSort);
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::onAddTimeSort(const bool& down) {
    if(down)ui->local_sort_toolButton->setToolTip("当前排序方式：添加时间降序");
    else ui->local_sort_toolButton->setToolTip("当前排序方式：添加时间升序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item;
    }
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则
    auto addTimeSort = [down](const MusicItemWidget* item1,const MusicItemWidget* item2) {
        if(down) return item1->m_information.addTime > item2->m_information.addTime;
        return item1->m_information.addTime < item2->m_information.addTime;
    };
    std::sort(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(),addTimeSort);
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::onSongNameSort(const bool& down) {
    if(down)ui->local_sort_toolButton->setToolTip("当前排序方式：歌曲名降序");
    else ui->local_sort_toolButton->setToolTip("当前排序方式：歌曲名升序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item;
    }
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则
    auto songNameSort = [down](const MusicItemWidget* item1,const MusicItemWidget* item2) {
        if(down) return item1->m_information.songName > item2->m_information.songName;
        return item1->m_information.songName < item2->m_information.songName;
    };
    std::sort(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(),songNameSort);
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    //qDebug()<<"更新";
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::onSingerSort(const bool& down) {
    if(down)ui->local_sort_toolButton->setToolTip("当前排序方式：歌手降序");
    else ui->local_sort_toolButton->setToolTip("当前排序方式：歌手升序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item;
    }
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则
    auto singerSort = [down](const MusicItemWidget* item1,const MusicItemWidget* item2) {
        if(down) return item1->m_information.singer > item2->m_information.singer;
        return item1->m_information.singer < item2->m_information.singer;
    };
    std::sort(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(),singerSort);
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    //qDebug()<<"更新";
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::onDurationSort(const bool& down) {
    if(down)ui->local_sort_toolButton->setToolTip("当前排序方式：时长降序");
    else ui->local_sort_toolButton->setToolTip("当前排序方式：时长升序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item;
    }
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则
    auto durationSort = [down](const MusicItemWidget* item1,const MusicItemWidget* item2) {
        if(down) return item1->m_information.duration > item2->m_information.duration;
        return item1->m_information.duration < item2->m_information.duration;
    };
    std::sort(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(),durationSort);
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    //qDebug()<<"更新";
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::onPlayCountSort(const bool& down) {
    if(down)ui->local_sort_toolButton->setToolTip("当前排序方式：播放次数降序");
    else ui->local_sort_toolButton->setToolTip("当前排序方式：播放次数升序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item;
    }
    layout->setSpacing(2);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则
    auto playCountSort = [down](const MusicItemWidget* item1,const MusicItemWidget* item2) {
        if(down) return item1->m_information.playCount > item2->m_information.playCount;
        return item1->m_information.playCount < item2->m_information.playCount;
    };
    std::sort(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(),playCountSort);
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::onRandomSort() {
    ui->local_sort_toolButton->setToolTip("当前排序方式：随机排序");
    ui->local_song_list_widget->setUpdatesEnabled(false);
    auto layout = ui->local_song_list_widget->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item;
    }
    layout->setSpacing(0);
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->setContentsMargins(0,0,0,0);
    //排序规则(打乱)
    // 使用当前时间作为随机数种子
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    // 随机打乱 QVector
    std::shuffle(this->m_MusicItemVector.begin(), this->m_MusicItemVector.end(), std::default_random_engine(seed));
    int index = 0;
    for(auto& val : this->m_MusicItemVector) {
        val->setIndexText(++index);
        dynamic_cast<QVBoxLayout*>(ui->local_song_list_widget->layout())->insertWidget(ui->local_song_list_widget->layout()->count() - 1, val);
    }
    // 恢复更新
    ui->local_song_list_widget->setUpdatesEnabled(true);
    update();
    emit syncSongInfo(this->m_locationMusicVector);
}

void LocalDownload::on_local_sort_toolButton_clicked() {
    getMenuPosition(QCursor::pos());
    this->m_sortOptMenu->move(this->m_menuPosition);
    this->m_sortOptMenu->show();
}
