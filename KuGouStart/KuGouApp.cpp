#include "KuGouApp.h"
#include "ui_KuGouApp.h"

#include<QMediaMetaData>
#include<QMediaPlayer>
#include<QAudioOutput>
#include<QPainterPath>
#include<QPainter>
#include<QPixmap>
#include<QDir>
#include<QFile>
#include<QPoint>
#include<QMouseEvent>
#include<QButtonGroup>
#include<QSizeGrip>
#include<QPropertyAnimation>
#include<QScrollBar>
#include <QTimer>
#include <QShortcut>
constexpr int SHADOW_WIDTH = 5;
constexpr int RADIUS = 8;

QPixmap roundedPixmap(const QPixmap &src, QSize size, int radius) {
    QPixmap scaled = src.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap dest(size);
    dest.fill(Qt::transparent);

    QPainter painter(&dest);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, size.width(), size.height(), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaled);

    return dest;
}

KuGouApp::KuGouApp(MainWindow *parent)
    : MainWindow(parent)
      , ui(new Ui::KuGouApp)
      , m_player(std::make_unique<QMediaPlayer>(this))
      , m_audioOutput(std::make_unique<QAudioOutput>(this))
      , m_menuBtnGroup(std::make_unique<QButtonGroup>(this))
      , m_sizeGrip(std::make_unique<QSizeGrip>(this))
      , m_upBtn(std::make_unique<UpToolButton>(this))
      , m_animation(std::make_unique<QPropertyAnimation>(this, "geometry"))
      , m_recommendForYou(std::make_unique<RecommendForYou>(this))
      , m_localDownload(std::make_unique<LocalDownload>(this))
      , m_scrollBarTimer(new QTimer(this)) {
    ui->setupUi(this);
    QFile file(QStringLiteral("://Res/styles/original.css"));
    if (file.open(QIODevice::ReadOnly)) {
        this->setStyleSheet(file.readAll());
    } else {
        qDebug() << "样式表打开失败QAQ";
        return;
    }
    initUi();
    this->m_player->setAudioOutput(this->m_audioOutput.get());
    this->m_audioOutput->setVolume(0.2);
    connect(ui->volume_toolButton, &VolumeToolBtn::volumeChange, this, [this](const int value) {
        const float volume = static_cast<float>(value) / 100; // 将值转换为0.0到1.0之间
        this->m_audioOutput->setVolume(volume); // 设置音量
    });

    // 设置快捷键
    new QShortcut(QKeySequence("Space"), this, SLOT(onKeyPause())); // 空格键暂停/播放
    new QShortcut(QKeySequence("Right"), this, SLOT(onKeyRight())); // 右箭头快进
    new QShortcut(QKeySequence("Left"), this, SLOT(onKeyLeft())); // 左箭头快退

    connect(this->m_player.get(), &QMediaPlayer::positionChanged, this, [this](int position) {
        if (ui->progressSlider->isSliderDown())return;
        //qDebug()<<"position "<<position;
        ui->progressSlider->setValue(position);
        ui->position_label->setText(QTime::fromMSecsSinceStartOfDay(position).toString("mm:ss"));
    });
    connect(this->m_player.get(), &QMediaPlayer::durationChanged, this, &KuGouApp::updateSliderRange);
    connect(this->m_player.get(), &QMediaPlayer::metaDataChanged, this, [this] {
        //qDebug() << "metaDataChanged";
        if (this->m_isOrderPlay) {
            ui->cover_label->setPixmap(roundedPixmap(this->m_songInfoVector[this->m_orderIndex].cover,
                                                     ui->cover_label->size(), 8));
            ui->song_name_label->setText(this->m_songInfoVector[this->m_orderIndex].songName);
            ui->singer_label->setText(this->m_songInfoVector[this->m_orderIndex].singer);
        } else {
            ui->cover_label->setPixmap(roundedPixmap(this->m_songInfoVector[this->m_songIndex].cover,
                                                     ui->cover_label->size(), 8));
            ui->song_name_label->setText(this->m_songInfoVector[this->m_songIndex].songName);
            ui->singer_label->setText(this->m_songInfoVector[this->m_songIndex].singer);
        }
    });
    connect(this->m_player.get(), &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState)this->m_isPlaying = true;
        else this->m_isPlaying = false;
        if (this->m_isPlaying) {
            ui->play_or_pause_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/pause.svg")));
        } else {
            ui->play_or_pause_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/play.svg")));
        }
    });
    mediaStatusConnection = connect(this->m_player.get(), &QMediaPlayer::mediaStatusChanged, this,
                                    [=](const QMediaPlayer::MediaStatus &status) {
                                        if (status == QMediaPlayer::EndOfMedia) {
                                            if (this->m_isOrderPlay) {
                                                //qDebug()<<"结束，开始播放下一首";
                                                addOrderIndex();
                                            }
                                        }
                                    });

    connect(ui->progressSlider, &QSlider::sliderReleased, this, &KuGouApp::updateProcess);
    connect(ui->progressSlider, &QSlider::sliderMoved, this, &KuGouApp::updateProcess);

    connect(this->m_localDownload.get(), &LocalDownload::playMusic, this, &KuGouApp::onPlayMusic);
    connect(this->m_localDownload.get(), &LocalDownload::startPlay, this, &KuGouApp::onStartPlay);
    connect(this->m_localDownload.get(), &LocalDownload::addSongInfo, this, &KuGouApp::onAddSongInfo);

    connect(this, &KuGouApp::setPlayIndex, this->m_localDownload.get(), &LocalDownload::setPlayIndex);

    connect(this->m_upBtn.get(), &QToolButton::clicked, this, &KuGouApp::onUpBtnClicked);

    //专门处理upBtn
    this->m_vScrollBar = ui->context_scrollArea->verticalScrollBar();
    connect(this->m_vScrollBar, &QScrollBar::valueChanged, this, &KuGouApp::onScrollBarValueChanged);
    connect(this->m_scrollBarTimer, &QTimer::timeout, this, &KuGouApp::onUpBtnShowOrNot);


    //专门处理重排
    connect(this->m_localDownload.get(),&LocalDownload::syncSongInfo,this,&KuGouApp::onSyncSongInfoVector);

    ui->progressSlider->installEventFilter(this);
}

KuGouApp::~KuGouApp() {
    delete ui;
}

void KuGouApp::initUi() {
    this->setWindowIcon(QIcon(QStringLiteral("://Res/window/windowIcon.svg")));
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    //移动窗口到合适的地方
    move(QGuiApplication::primaryScreen()->geometry().width() / 2 - this->width() / 2, 100);

    //设置鼠标追踪
    this->setMouseTracking(true);
    ui->title_widget->setMouseTracking(true);
    ui->center_widget->setMouseTracking(true);
    ui->play_widget->setMouseTracking(true);
    //设置窗口属性
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_Hover, true);
    //设置滚动定时器
    this->m_scrollBarTimer->setSingleShot(true); // 只触发一次

    initTitleWidget();
    initCommendForYouWidget();
    initLocalDownload();
    initPlayWidget();
    initMenu();
    initLocalDownload();
    initCornerWidget();
}

void KuGouApp::initCommendForYouWidget() {
    ui->stackedWidget->addWidget(this->m_recommendForYou.get());
}

void KuGouApp::initLocalDownload() {
    ui->stackedWidget->addWidget(this->m_localDownload.get());
}

void KuGouApp::initTitleWidget() {
    ui->index_label1->setPixmap(
        QPixmap(QStringLiteral("://Res/titlebar/h-line.png")).scaled(30, 15, Qt::KeepAspectRatio));
    ui->index_label2->setPixmap(
        QPixmap(QStringLiteral("://Res/titlebar/h-line.png")).scaled(30, 15, Qt::KeepAspectRatio));
    ui->index_label3->setPixmap(
        QPixmap(QStringLiteral("://Res/titlebar/h-line.png")).scaled(30, 15, Qt::KeepAspectRatio));
    ui->index_label4->setPixmap(
        QPixmap(QStringLiteral("://Res/titlebar/h-line.png")).scaled(30, 15, Qt::KeepAspectRatio));
    ui->index_label2->hide();
    ui->index_label3->hide();
    ui->index_label4->hide();

    ui->title_line->setPixmap(QPixmap(QStringLiteral(":/Res/tabIcon/line-gray.svg")));
    ui->search_lineEdit->addAction(QIcon(QStringLiteral("://Res/titlebar/search-black.svg")),
                                   QLineEdit::LeadingPosition);

    //除非自定义QToolButton否则达不到 CSS 中 border-image 的效果
    //ui->listen_toolButton->setIcon(QIcon("://Res/titlebar/listen-music-black.svg"));

    QPixmap roundedPix = roundedPixmap(QPixmap(QStringLiteral("://Res/window/portrait.jpg")),
                                       ui->title_portrait_label->size(), 20);
    // 设置圆角半径
    ui->title_portrait_label->setPixmap(roundedPix);

    ui->title_gender_label->setPixmap(QPixmap(QStringLiteral("://Res/window/boy.svg")));

    //设置设置按钮的Frame圆角，填充颜色
    ui->min_toolButton->setRadius(6);
    ui->max_toolButton->setRadius(6);
    ui->close_toolButton->setRadius(6);

    ui->min_toolButton->setFillColor(QColor(QStringLiteral("#969696")));
    ui->max_toolButton->setFillColor(QColor(QStringLiteral("#969696")));
    ui->close_toolButton->setFillColor(QColor(QStringLiteral("#FF0066")));

    ui->min_toolButton->setMyIcon(QIcon(QStringLiteral("://Res/titlebar/minimize-black.svg")));
    ui->max_toolButton->setMyIcon(QIcon(QStringLiteral("://Res/titlebar/maximize-black.svg")));
    ui->close_toolButton->setMyIcon(QIcon(QStringLiteral("://Res/titlebar/close-black.svg")));

    connect(ui->title_widget, &TitleWidget::doubleClicked, this, [this] { ui->max_toolButton->click(); });
}

void KuGouApp::initPlayWidget() {
    ui->love_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/collect.svg")));
    ui->download_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/download.svg")));
    ui->comment_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/comment.svg")));
    ui->share_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/share.svg")));
    ui->more_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/more.svg")));
    ui->pre_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/previous-song.svg")));
    ui->play_or_pause_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/play.svg")));
    ui->next_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/next-song.svg")));
    ui->erji_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/together.svg")));
    ui->ci_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/song-words.svg")));
    ui->list_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/play-list.svg")));

    connect(ui->play_widget, &PlayWidget::doubleClicked, this, [this] { ui->max_toolButton->clicked(); });
}

void KuGouApp::initMenu() {
    this->m_menuBtnGroup->setParent(ui->center_menu_widget);
    ui->recommend_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/recommend.svg")));
    ui->yueku_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/music-library.svg")));
    ui->gedan_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/song-list.svg")));
    ui->pindao_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/my-channel.svg")));
    ui->video_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/video.svg")));
    ui->live_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/live.svg")));
    ui->my_shoucang_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/collect.svg")));
    ui->my_pindao_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/my-channel.svg")));
    ui->local_download_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/download.svg")));
    ui->music_yunpan_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/cloud.svg")));
    ui->yigou_yunpan_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/bought.svg")));
    ui->zuijin_bofang_toolButton->setIcon(QIcon(QStringLiteral("://Res/window/history.svg")));
    ui->moren_liebiao_toolButton->setIcon(QIcon(QStringLiteral("://Res/titlebar/menu-black.svg")));

    m_menuBtnGroup->addButton(ui->recommend_toolButton);
    m_menuBtnGroup->addButton(ui->yueku_toolButton);
    m_menuBtnGroup->addButton(ui->gedan_toolButton);
    m_menuBtnGroup->addButton(ui->pindao_toolButton);
    m_menuBtnGroup->addButton(ui->video_toolButton);
    m_menuBtnGroup->addButton(ui->live_toolButton);
    m_menuBtnGroup->addButton(ui->my_shoucang_toolButton);
    m_menuBtnGroup->addButton(ui->my_pindao_toolButton);
    m_menuBtnGroup->addButton(ui->local_download_toolButton);
    m_menuBtnGroup->addButton(ui->music_yunpan_toolButton);
    m_menuBtnGroup->addButton(ui->yigou_yunpan_toolButton);
    m_menuBtnGroup->addButton(ui->zuijin_bofang_toolButton);
    m_menuBtnGroup->addButton(ui->moren_liebiao_toolButton);
    m_menuBtnGroup->setExclusive(true);
}

void KuGouApp::initCornerWidget() {
    this->m_sizeGrip->setFixedSize(11, 11);
    this->m_sizeGrip->setObjectName(QStringLiteral("sizegrip"));
}

void KuGouApp::getCurrentIndex(int& index) {
    SongInfor temp;
    temp.songName = this->m_lastSongInfoVector[index].songName;
    temp.singer = this->m_lastSongInfoVector[index].singer;
    temp.duration = this->m_lastSongInfoVector[index].duration;
    auto index1 = std::find(m_lastSongInfoVector.begin(), m_lastSongInfoVector.end(),temp);

    index = static_cast<int>(index1 - m_lastSongInfoVector.begin());
}

void KuGouApp::addOrderIndex() {
    this->m_orderIndex = (this->m_orderIndex + 1) % static_cast<int>(this->m_songInfoVector.size());
    this->m_songIndex = this->m_orderIndex;
    setPlayMusic(this->m_orderIndex);
}

void KuGouApp::subOrderIndex() {
    int s = static_cast<int>(this->m_songInfoVector.size());
    this->m_orderIndex = (this->m_orderIndex + s - 1) % s;
    this->m_songIndex = this->m_orderIndex;
    setPlayMusic(this->m_orderIndex);
}

void KuGouApp::addSongIndex() {
    this->m_songIndex = (this->m_songIndex + 1) % static_cast<int>(this->m_songInfoVector.size());;
    setPlayMusic(this->m_songIndex);
}

void KuGouApp::subSongIndex() {
    int s = static_cast<int>(this->m_songInfoVector.size());
    this->m_songIndex = (this->m_songIndex + s - 1) % s;;
    setPlayMusic(this->m_songIndex);
}

void KuGouApp::mousePressEvent(QMouseEvent *ev) {
    MainWindow::mousePressEvent(ev);
    if (ev->button() == Qt::LeftButton) {
        this->m_pressPos = ev->pos();
        // 获取鼠标在那个区域
    }
}

void KuGouApp::mouseMoveEvent(QMouseEvent *event) {
    MainWindow::mouseMoveEvent(event);
    if (this->m_isTransForming)return;
    // 计算的鼠标移动偏移量, 就是鼠标全局坐标 - 减去点击时鼠标坐标
    QPoint point_offset = event->globalPosition().toPoint() - mousePs;

    if ((event->buttons() == Qt::LeftButton) && isPress) {
        if (this->m_isMaxScreen == true) //如果是最大化拖动，则还原大小再拖动
        {
            this->endGeometry = this->startGeometry;
            this->endGeometry.setX(event->pos().x() - this->startGeometry.width() / 2);
            this->endGeometry.setY(event->pos().y() - 3);
            this->startGeometry = this->endGeometry;
            //qDebug()<<"点击pos : "<<event->pos()<<" start :"<<startGeometry;
            ui->max_toolButton->clicked();
            return;
        }
        if (mouse_press_region == kMousePositionMid) {
            if (ui->title_widget->geometry().contains(this->m_pressPos) ||
                ui->play_widget->geometry().contains(this->m_pressPos)) {
                move(windowsLastPs + point_offset);
            }
        } else {
            // 其他部分 是拉伸窗口
            // 获取客户区
            QRect rect = geometry();
            switch (mouse_press_region) {
                // 左上角
                case kMousePositionLeftTop:
                    rect.setTopLeft(rect.topLeft() + point_offset);
                    break;
                case kMousePositionTop:
                    rect.setTop(rect.top() + point_offset.y());
                    break;
                case kMousePositionRightTop:
                    rect.setTopRight(rect.topRight() + point_offset);
                    break;
                case kMousePositionRight:
                    rect.setRight(rect.right() + point_offset.x());
                    break;
                case kMousePositionRightBottom:
                    rect.setBottomRight(rect.bottomRight() + point_offset);
                    break;
                case kMousePositionBottom:
                    rect.setBottom(rect.bottom() + point_offset.y());
                    break;
                case kMousePositionLeftBottom:
                    rect.setBottomLeft(rect.bottomLeft() + point_offset);
                    break;
                case kMousePositionLeft:
                    rect.setLeft(rect.left() + point_offset.x());
                    break;
                default:
                    break;
            }
            setGeometry(rect);
            mousePs = event->globalPosition().toPoint();
        }
    }
}

void KuGouApp::paintEvent(QPaintEvent *ev) {
    QWidget::paintEvent(ev);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QBrush brush(QColor(QStringLiteral("#eef2ff")));
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);
    QRect rect = this->rect();
    QPainterPath path;
    path.addRoundedRect(rect, RADIUS, RADIUS);
    painter.drawPath(path);
    //------------绘制阴影
    /*QPainterPath path1;
    path1.setFillRule(Qt::WindingFill);
    path1.addRoundedRect(SHADOW_WIDTH,SHADOW_WIDTH, this->width() - SHADOW_WIDTH * 2, this->height() - SHADOW_WIDTH * 2,RADIUS,RADIUS);
    QColor color(150, 150, 150, 55);
    for (int i = 0; i != SHADOW_WIDTH; ++i)
    {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        path.addRoundedRect(SHADOW_WIDTH - i, SHADOW_WIDTH- i, this->width() - (SHADOW_WIDTH- i) * 2, this->height() - (SHADOW_WIDTH- i) * 2, RADIUS, RADIUS);
        color.setAlpha(180 - static_cast<int>(qSqrt(i) * 80));
        painter.setPen(color);
        painter.drawPath(path);
    }*/
}

void KuGouApp::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    //角标移动
    this->m_sizeGrip->move(this->width() - this->m_sizeGrip->width() - 3,
                           this->height() - this->m_sizeGrip->height() - 3);
    this->m_sizeGrip->raise();
    this->m_sizeGrip->setVisible(true);
    //UpWidget移动
    this->m_upBtn->move(this->width() - this->m_upBtn->width() - 20,
                        this->height() - this->m_upBtn->height() - 100);
    this->m_upBtn->raise();
}

bool KuGouApp::event(QEvent *event) {
    if (QEvent::HoverMove == event->type()) //鼠标移动
    {
        //auto ev = dynamic_cast<QMouseEvent*>(event);
        //if (ev) {
        //    this->mouseMoveEvent(ev);
        //}

        // 此处最好用static_cast
        auto ev = static_cast<QMouseEvent *>(event);
        this->mouseMoveEvent(ev);
    }

    return QWidget::event(event);
}

bool KuGouApp::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->progressSlider) {
        // 禁用进度条拖拽，当播放器没有加载资源时
        // 如果播放器没有加载资源，禁用进度条的鼠标事件
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseMove) {
            if (this->m_player->source().isEmpty()) {
                return true; // 只拦截与鼠标有关的事件
            }
        }
        if (event->type() == QEvent::MouseButtonPress) //判断类型
        {
            auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) //判断左键
            {
                //qDebug() << "触发点击";
                qint64 value = QStyle::sliderValueFromPosition(ui->progressSlider->minimum(),
                                                               ui->progressSlider->maximum(), mouseEvent->pos().x(),
                                                               ui->progressSlider->width());
                {
                    //水平进度条动态地划到点击位置
                    //auto ani = new QPropertyAnimation(ui->progressSlider,"sliderPosition");
                    // 设置动画持续时间，单位是毫秒，这里设置为100毫秒
                    //ani->setDuration(100);
                    // 动画开始值为滑块的当前位置
                    //ani->setStartValue(ui->progressSlider->value());
                    // 动画结束值为你希望的目标值
                    //ani->setEndValue(value);
                    // 设置动画的曲线类型，比如缓入缓出的效果
                    //ani->setEasingCurve(QEasingCurve::Linear);
                    // 启动动画
                    //ani->start(QAbstractAnimation::DeleteWhenStopped);
                }

                this->m_player->setPosition(value);
                if (!this->m_isPlaying)ui->play_or_pause_toolButton->clicked();
            }
        }
    }
    return MainWindow::eventFilter(watched, event);
}

void KuGouApp::setPlayMusic(int &index) {
    qDebug()<<"之前是第 "<<index<<" 首";
    getCurrentIndex(index);
    qDebug()<<"现在是第 "<<index<<" 首";
    emit setPlayIndex(index);
    this->m_player->stop();
    this->m_player->setSource(QUrl(this->m_songInfoVector[index].mediaPath));
    this->m_player->play();
}

void KuGouApp::updateProcess() {
    //qDebug()<<"sliderMoved / sliderReleased : "<<ui->progressSlider->value();
    qint64 position = ui->progressSlider->value() * this->m_player->duration() / ui->progressSlider->maximum();
    this->m_player->setPosition(position);
    this->m_player->play();
}

void KuGouApp::updateSliderRange(const qint64 &duration) {
    ui->progressSlider->setMaximum(static_cast<int>(duration));
    //qDebug()<<"改变总时长";
    ui->duration_label->setText(QTime::fromMSecsSinceStartOfDay(static_cast<int>(duration)).toString("mm:ss"));
}

void KuGouApp::onPlayMusic(const int &index) {
    this->m_isOrderPlay = false;
    this->m_songIndex = index - 1;
    setPlayMusic(this->m_songIndex);
}

void KuGouApp::onStartPlay() {
    this->m_isOrderPlay = true;
    this->m_orderIndex = 0;
    //直接从第一首开始播放
    setPlayMusic(this->m_orderIndex);
}

void KuGouApp::onAddSongInfo(const SongInfor &info) {
    this->m_songInfoVector.emplace_back(info);
    this->m_lastSongInfoVector.emplace_back(info);
}

void KuGouApp::onUpBtnClicked() {
    // 创建 QPropertyAnimation 对象
    // 获取 QScrollArea 的垂直滚动条
    this->m_vScrollBar = ui->context_scrollArea->verticalScrollBar();

    // 标记动画开始
    ui->context_scrollArea->setAnimating(true); //开始禁用滚轮

    auto animation = new QPropertyAnimation(this->m_vScrollBar, "value", this);
    // 设置动画的起始值（当前滚动条位置）和结束值（最顶部）
    animation->setStartValue(this->m_vScrollBar->value()); // 当前滚动条位置
    animation->setEndValue(0); // 滚动到顶部（0 表示最上方）
    animation->setDuration(500); // 动画持续时间，500ms
    animation->setEasingCurve(this->m_curves); // 缓动曲线

    // 在动画结束后标记动画停止
    connect(animation, &QPropertyAnimation::finished, this, [this]() {
        ui->context_scrollArea->setAnimating(false); //动画结束启用滚轮
    });

    // 启动动画
    animation->start(QAbstractAnimation::DeleteWhenStopped); // 动画结束后自动删除
}

void KuGouApp::onScrollBarValueChanged(const int &value) {
    // 启动定时器，延迟处理
    if (!this->m_scrollBarTimer->isActive()) {
        this->m_scrollBarTimer->start(500); // 500ms 延迟，避免过于频繁地触发
    }
    this->m_scrollValue = value;
}

void KuGouApp::onUpBtnShowOrNot() {
    //qDebug()<<this->m_scrollValue;
    if (this->m_scrollValue > 120)this->m_upBtn->show();
    else this->m_upBtn->hide();
}

void KuGouApp::onKeyPause() {
    if (this->m_player->playbackState() == QMediaPlayer::PlaybackState::PlayingState) {
        this->m_player->pause();
    } else {
        this->m_player->play();
    }
}

void KuGouApp::onKeyLeft() {
    qint64 pos = this->m_player->position() - 5000;
    this->m_player->setPosition(pos < 0 ? 0 : pos);
}

void KuGouApp::onKeyRight() {
    qint64 pos = this->m_player->position() + 5000;
    this->m_player->setPosition(pos > this->m_player->duration() ? this->m_player->duration() : pos);
}

void KuGouApp::onSyncSongInfoVector(QVector<SongInfor> &vec) {
    qDebug()<<"重排，更新";
    //保留旧的
    this->m_lastSongInfoVector = this->m_songInfoVector;
    //获取同步的播放顺序
    this->m_songInfoVector = vec;
}

void KuGouApp::on_title_return_toolButton_clicked() {
    qDebug() << "返回，估计要使用堆栈";
}

void KuGouApp::on_title_refresh_toolButton_clicked() {
    qDebug() << "刷新界面";
    ui->center_widget->repaint();
}

void KuGouApp::on_title_music_pushButton_clicked() {
    ui->index_label1->show();
    ui->index_label2->hide();
    ui->index_label3->hide();
    ui->index_label4->hide();
}

void KuGouApp::on_title_live_pushButton_clicked() {
    ui->index_label1->hide();
    ui->index_label2->show();
    ui->index_label3->hide();
    ui->index_label4->hide();
}

void KuGouApp::on_title_listenBook_pushButton_clicked() {
    ui->index_label1->hide();
    ui->index_label2->hide();
    ui->index_label3->show();
    ui->index_label4->hide();
}

void KuGouApp::on_title_found_pushButton_clicked() {
    ui->index_label1->hide();
    ui->index_label2->hide();
    ui->index_label3->hide();
    ui->index_label4->show();
}

void KuGouApp::on_min_toolButton_clicked() {
    //QRect startGeometry = this->geometry(); // 获取当前窗口的几何形状(正常状态)
    //QRect endGeometry = startGeometry;
    ////endGeometry.setWidth(0); // 设置目标宽度为0
    //endGeometry.setHeight(100); // 设置目标高度为0
    //this->m_animation->setDuration(1000); // 动画时长
    //this->m_animation->setStartValue(startGeometry);
    //this->m_animation->setEndValue(endGeometry);
    //this->m_animation->setEasingCurve(QEasingCurve::InOutQuad); // 设置动画曲线
    //this->m_animation->start(); // 启动动画
    //// 最小化窗口
    //connect(this->m_animation.get(), &QPropertyAnimation::finished, this, [&]() {this->showMinimized();});
    this->showMinimized();
}

void KuGouApp::on_max_toolButton_clicked() {
    if (m_isMaxScreen) {
        this->m_isMaxScreen = false;
        endGeometry = startGeometry; // 获取普通尺寸时的几何形状
        startGeometry = this->screen()->availableGeometry();
        //this->setGeometry(startGeometry); // 恢复前，我们先把它设置回最大化尺寸
        this->m_maxBtnStyle = R"(QToolButton#max_toolButton {
                                background-color: rgba(255,255,255,0);
                                qproperty-icon: url("://Res/titlebar/maximize-black.svg");
                                border-radius: 6px;
                                height: 30px;
                                width: 30px;
                                icon-size: 12px 12px;
                            })";
        ui->max_toolButton->setMyIcon(QIcon("://Res/titlebar/maximize-black.svg"));
        this->m_animation->setDuration(500); // 设置动画持续时间
    }
    else {
        this->m_isMaxScreen = true;
        // 如果当前不是最大化状态，则目标是最大化
        startGeometry = this->geometry();
        endGeometry = this->screen()->availableGeometry(); // 获取屏幕的最大化尺寸
        this->m_maxBtnStyle = R"(QToolButton#max_toolButton {
                                background-color: rgba(255,255,255,0);
                                qproperty-icon: url("://Res/titlebar/resume-black.svg");
                                border-radius: 6px;
                                height: 30px;
                                width: 30px;
                                icon-size: 12px 12px;
                            })";
        ui->max_toolButton->setMyIcon(QIcon("://Res/titlebar/resume-black.svg"));
        this->m_animation->setDuration(400); // 设置动画持续时间
    }
    //qDebug()<<"start : "<<this->startGeometry<<" end : "<<this->endGeometry;
    this->m_animation->setStartValue(startGeometry); // 动画开始时的窗口几何
    this->m_animation->setEndValue(endGeometry); // 动画结束时的窗口几何
    this->m_animation->setEasingCurve(QEasingCurve::InOutQuad); // 设置动画的缓动曲线

    this->m_isTransForming = true; // 正在变形，禁用点击、拖动事件
    // 开始动画
    this->m_animation->start();
    // 在动画结束后标记动画停止
    connect(this->m_animation.get(), &QPropertyAnimation::finished, this, [this] {
        // 动画结束后，延迟 500 毫秒启用拖拽
        QTimer::singleShot(500, this, [this] {
            this->m_isTransForming = false; // 启用拖拽
        });
    });

    // 更新按钮样式
    ui->max_toolButton->setStyleSheet(this->m_maxBtnStyle);
}

void KuGouApp::on_close_toolButton_clicked() {
    QApplication::quit(); // 退出应用程序
}

void KuGouApp::on_recommend_toolButton_clicked() {
    ui->stackedWidget->setCurrentWidget(this->m_recommendForYou.get());
    this->m_curves = QEasingCurve::OutBounce; // 缓动曲线
    this->m_upBtn->clicked();
}

void KuGouApp::on_local_download_toolButton_clicked() {
    ui->stackedWidget->setCurrentWidget(this->m_localDownload.get());
    this->m_curves = QEasingCurve::OutQuart; // 缓动曲线
    this->m_upBtn->clicked();
}

void KuGouApp::on_play_or_pause_toolButton_clicked() {
    //如果未设置播放源就return
    if (this->m_player->source().isEmpty()) return;
    this->m_isPlaying = !this->m_isPlaying;
    if (this->m_isPlaying) {
        this->m_player->play();
        ui->play_or_pause_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/pause.svg")));
    } else {
        this->m_player->pause();
        ui->play_or_pause_toolButton->setIcon(QIcon(QStringLiteral("://Res/playbar/play.svg")));
    }
}

void KuGouApp::on_love_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
}

void KuGouApp::on_download_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
}

void KuGouApp::on_comment_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
}

void KuGouApp::on_share_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
}

void KuGouApp::on_more_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
}

void KuGouApp::on_circle_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
    m_isSingleCircle = !m_isSingleCircle;
    if (m_isSingleCircle) {
        //qDebug()<<"单曲循环";
        //this->m_player->setLoops(QMediaPlayer::Loops::Infinite);
        ////怪不得，原来错在这里，我就说怎么循环播放进度条一直有问题，服了
        ui->circle_toolButton->setStyleSheet(
            R"(QToolButton{border-image:url('://Res/playbar/single-list-loop-gray.svg');}
                                            QToolButton:hover{border-image:url('://Res/playbar/single-list-loop-blue.svg');})");
        if (mediaStatusConnection) {
            disconnect(mediaStatusConnection);
            mediaStatusConnection = connect(this->m_player.get(), &QMediaPlayer::mediaStatusChanged, this,
                                            [=](QMediaPlayer::MediaStatus status) {
                                                if (status == QMediaPlayer::EndOfMedia) {
                                                    //qDebug()<<"播放结束";
                                                    // 当播放结束时，重新开始播放
                                                    //qDebug()<<"循环播放 ："<<this->m_isSingleCircle;
                                                    this->m_player->stop(); // 设置到文件的开头
                                                    //this->m_player->play();
                                                    setPlayIndex(this->m_songIndex);
                                                }
                                            });
        } else
            qDebug() << "mediaStatusConnection is empty";
    } else {
        //qDebug()<<"播放一次";
        if (mediaStatusConnection) {
            disconnect(mediaStatusConnection);
            mediaStatusConnection = connect(this->m_player.get(), &QMediaPlayer::mediaStatusChanged, this,
                                            [=](QMediaPlayer::MediaStatus status) {
                                                if (status == QMediaPlayer::EndOfMedia) {
                                                    if (this->m_isOrderPlay) {
                                                        //qDebug()<<"结束，开始播放下一首";
                                                        addOrderIndex();
                                                    }
                                                }
                                            });
        } else
            qDebug() << "mediaStatusConnection is empty";

        ui->circle_toolButton->setStyleSheet(R"(QToolButton{border-image:url('://Res/playbar/list-loop-gray.svg');}
                                            QToolButton:hover{border-image:url('://Res/playbar/list-loop-blue.svg');})");
    }
}

void KuGouApp::on_pre_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
    if (this->m_isOrderPlay) subOrderIndex();
    else subSongIndex();
}

void KuGouApp::on_next_toolButton_clicked() {
    if (this->m_player->source().isEmpty()) return;
    if (this->m_isOrderPlay) addOrderIndex();
    else addSongIndex();
}
