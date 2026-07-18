#include "SolitaireGame.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <algorithm>
#include <random>
#include <cmath>

// ========================= КОНСТРУКТОР =========================
SolitaireGame::SolitaireGame(QWidget *parent) : QWidget(parent) {
    setWindowTitle("Пасьянс Клондайк");
    setMinimumSize(1100, 750);
    setMouseTracking(true);
    setStyleSheet("background-color: #0b3d1f;");

    // ===== Главный layout =====
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== Верхняя панель =====
    topBar = new QWidget(this);
    topBar->setFixedHeight(topBarH);
    topBar->setStyleSheet(
        "background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #1a5c2e, stop:1 #0b3d1f);"
        "border-bottom: 2px solid #2d8a47;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 0, 20, 0);

    lblMoves = new QLabel("Ходов: 0", topBar);
    lblMoves->setStyleSheet(
        "color: #f0e68c; font-size: 18px; font-weight: bold;");

    btnNewGame = new QPushButton("🔄 Новая игра", topBar);
    btnBackToMenu = new QPushButton("◀ В меню", topBar);

    for (auto btn : {btnNewGame, btnBackToMenu}) {
        btn->setFixedSize(150, 40);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background-color: #2d8a47; color: white; "
            "font-size: 14px; font-weight: bold; border-radius: 8px; "
            "border: 1px solid #4caf6a; }"
            "QPushButton:hover { background-color: #3aa55a; }"
            "QPushButton:pressed { background-color: #1f6b36; }");
    }

    topLayout->addWidget(lblMoves);
    topLayout->addStretch();
    topLayout->addWidget(btnNewGame);
    topLayout->addWidget(btnBackToMenu);
    topBar->hide();

    // ===== Игровая область (пустой виджет для paintEvent) =====
    QWidget *gameArea = new QWidget(this);
    gameArea->setStyleSheet("background: transparent;");

    mainLayout->addWidget(topBar);
    mainLayout->addWidget(gameArea, 1);

    // ===== Меню =====
    menuWidget = new QWidget(this);
    QVBoxLayout *menuLayout = new QVBoxLayout(menuWidget);
    menuLayout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel("♠ ♥ ПАСЬЯНС ♦ ♣", menuWidget);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "color: #f0e68c; font-size: 42px; font-weight: bold; "
        "margin-bottom: 30px;");

    QLabel *menuLabel = new QLabel("Выберите количество колод:", menuWidget);
    menuLabel->setAlignment(Qt::AlignCenter);
    menuLabel->setStyleSheet(
        "color: white; font-size: 20px; margin-bottom: 20px;");

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(20);
    btnRow->setAlignment(Qt::AlignCenter);

    auto makeDeckBtn = [&](const QString &text, int decks) {
        QPushButton *b = new QPushButton(text, menuWidget);
        b->setFixedSize(180, 70);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 #3498db, stop:1 #2176ad); color: white; font-size: 20px; "
            "font-weight: bold; border-radius: 12px; border: 2px solid #5dade2; }"
            "QPushButton:hover { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 #5dade2, stop:1 #3498db); }"
            "QPushButton:pressed { background-color: #1f6391; }");
        connect(b, &QPushButton::clicked, this, [this, decks]() { startGame(decks); });
        return b;
    };

    btnRow->addWidget(makeDeckBtn("1 колода", 1));
    btnRow->addWidget(makeDeckBtn("2 колоды", 2));
    btnRow->addWidget(makeDeckBtn("3 колоды", 3));

    menuLayout->addWidget(title);
    menuLayout->addWidget(menuLabel);
    menuLayout->addLayout(btnRow);

    QVBoxLayout *centerLayout = new QVBoxLayout(this);
    centerLayout->setAlignment(Qt::AlignCenter);
    centerLayout->addWidget(menuWidget);

    // ===== Кнопка "Играть" =====
    btnStart = new QPushButton("▶  ИГРАТЬ", this);
    btnStart->setFixedSize(240, 80);
    btnStart->setCursor(Qt::PointingHandCursor);
    btnStart->setStyleSheet(
        "QPushButton { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #27ae60, stop:1 #1e8449); color: white; font-size: 28px; "
        "font-weight: bold; border-radius: 15px; border: 3px solid #2ecc71; }"
        "QPushButton:hover { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #2ecc71, stop:1 #27ae60); }");
    centerLayout->addWidget(btnStart, 0, Qt::AlignCenter);

    // ===== Сигналы =====
    connect(btnStart, &QPushButton::clicked, this, [this]() {
        btnStart->hide();
        menuWidget->show();
    });
    connect(btnNewGame, &QPushButton::clicked, this, [this]() {
        startGame(numDecks);
    });
    connect(btnBackToMenu, &QPushButton::clicked, this, &SolitaireGame::backToMenu);
}

// ========================= ФОН =========================
void SolitaireGame::drawBackground(QPainter &p) {
    // Градиент стола
    QLinearGradient grad(0, topBarH, 0, height());
    grad.setColorAt(0, QColor(19, 90, 45));
    grad.setColorAt(1, QColor(8, 50, 25));
    p.fillRect(0, topBarH, width(), height() - topBarH, grad);

    // Декоративная рамка
    p.setPen(QPen(QColor(240, 230, 140, 60), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(15, topBarH + 15, width() - 30, height() - topBarH - 30, 15, 15);
}

// ========================= ОТРИСОВКА =========================
void SolitaireGame::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (gameState != PLAYING) return;

    drawBackground(p);

    // Stock
    drawCard(p, stockX, foundY,
             stock.empty() ? Card{-1, -1, false} : stock.back(),
             false, false);

    // Waste
    if (!waste.empty()) {
        bool hov = (hoverPos.x() >= wasteX && hoverPos.x() <= wasteX + cardW &&
                    hoverPos.y() >= foundY && hoverPos.y() <= foundY + cardH);
        drawCard(p, wasteX, foundY, waste.back(),
                 isSelected(1, 0, (int)waste.size() - 1), hov);
    }

    // Foundations
    for (int i = 0; i < (int)foundations.size(); ++i) {
        int fx = foundStartX + i * (cardW + spacing);
        if (foundations[i].empty()) {
            drawEmptySlot(p, fx, foundY, "A");
        } else {
            bool hov = (hoverPos.x() >= fx && hoverPos.x() <= fx + cardW &&
                        hoverPos.y() >= foundY && hoverPos.y() <= foundY + cardH);
            drawCard(p, fx, foundY, foundations[i].back(),
                     isSelected(3, i, (int)foundations[i].size() - 1), hov);
        }
    }

    // Tableau
    for (int i = 0; i < (int)tableau.size(); ++i) {
        int tx = tabStartX + i * (cardW + spacing);
        if (tableau[i].empty()) {
            drawEmptySlot(p, tx, tabY, "K");
        } else {
            int currentY = tabY;
            for (int j = 0; j < (int)tableau[i].size(); ++j) {
                // Проверяем hover для этой карты
                bool hov = (hoverPos.x() >= tx && hoverPos.x() <= tx + cardW &&
                            hoverPos.y() >= currentY && hoverPos.y() <= currentY + cardH);
                drawCard(p, tx, currentY, tableau[i][j],
                         isSelected(2, i, j), hov);
                currentY += tableau[i][j].faceUp ? tabOffset : tabOffsetDn;
            }
        }
    }
}

// ========================= КАРТА =========================
void SolitaireGame::drawCard(QPainter &p, int x, int y,
                             const Card &c, bool selected, bool hovered) {
    QRectF r(x, y, cardW, cardH);

    // Пустой слот-заглушка (рубашка stock)
    if (c.suit == -1) {
        p.setPen(QPen(QColor(240, 230, 140, 120), 2, Qt::DashLine));
        p.setBrush(QColor(255, 255, 255, 15));
        p.drawRoundedRect(r, 10, 10);
        p.setPen(QColor(240, 230, 140, 180));
        p.setFont(QFont("Arial", 30, QFont::Bold));
        p.drawText(r, Qt::AlignCenter, "↻");
        return;
    }

    // Тень
    if (selected || hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 80));
        p.drawRoundedRect(r.adjusted(4, 6, 4, 6), 10, 10);
    }

    if (!c.faceUp) {
        // Рубашка с градиентом и узором
        QLinearGradient grad(x, y, x + cardW, y + cardH);
        grad.setColorAt(0, QColor(25, 60, 120));
        grad.setColorAt(0.5, QColor(45, 90, 160));
        grad.setColorAt(1, QColor(25, 60, 120));
        p.setBrush(grad);
        p.setPen(QPen(QColor(200, 200, 255, 180), 1));
        p.drawRoundedRect(r, 10, 10);

        // Внутренняя рамка
        p.setPen(QPen(QColor(240, 230, 140, 200), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(6, 6, -6, -6), 6, 6);

        // Узор-ромб в центре
        p.setPen(QPen(QColor(240, 230, 140, 180), 2));
        QPointF center = r.center();
        QPolygonF diamond;
        diamond << QPointF(center.x(), center.y() - 20)
                << QPointF(center.x() + 15, center.y())
                << QPointF(center.x(), center.y() + 20)
                << QPointF(center.x() - 15, center.y());
        p.drawPolygon(diamond);
    } else {
        // Лицевая сторона
        p.setBrush(hovered ? QColor(255, 255, 240) : Qt::white);
        p.setPen(selected ? QPen(QColor(255, 215, 0), 3) : QPen(QColor(80, 80, 80), 1));
        p.drawRoundedRect(r, 10, 10);

        QString rankStr = (c.rank == 1) ? "A" : (c.rank == 11) ? "J"
                        : (c.rank == 12) ? "Q" : (c.rank == 13) ? "K"
                        : QString::number(c.rank);
        QString suitStr = (c.suit == 0) ? "♥" : (c.suit == 1) ? "♦"
                        : (c.suit == 2) ? "♣" : "♠";
        QColor textColor = (c.suit < 2) ? QColor(200, 30, 30) : QColor(20, 20, 20);

        // Ранг сверху-слева
        p.setPen(textColor);
        p.setFont(QFont("Arial", 18, QFont::Bold));
        p.drawText(r.adjusted(6, 4, 0, 0), Qt::AlignTop | Qt::AlignLeft, rankStr);
        p.setFont(QFont("Arial", 16));
        p.drawText(r.adjusted(6, 22, 0, 0), Qt::AlignTop | Qt::AlignLeft, suitStr);

        // Большая масть в центре
        p.setFont(QFont("Arial", 36));
        p.drawText(r, Qt::AlignCenter, suitStr);

        // Ранг снизу-справа (перевёрнутый)
        p.save();
        p.translate(r.center());
        p.rotate(180);
        p.setFont(QFont("Arial", 18, QFont::Bold));
        p.drawText(QRectF(-cardW/2 + 6, -cardH/2 + 4, cardW - 12, cardH/2),
                   Qt::AlignTop | Qt::AlignLeft, rankStr);
        p.setFont(QFont("Arial", 16));
        p.drawText(QRectF(-cardW/2 + 6, -cardH/2 + 22, cardW - 12, cardH/2),
                   Qt::AlignTop | Qt::AlignLeft, suitStr);
        p.restore();
    }
}

void SolitaireGame::drawEmptySlot(QPainter &p, int x, int y, const QString &symbol) {
    QRectF r(x, y, cardW, cardH);
    p.setPen(QPen(QColor(240, 230, 140, 100), 2, Qt::DashLine));
    p.setBrush(QColor(0, 0, 0, 25));
    p.drawRoundedRect(r, 10, 10);
    if (!symbol.isEmpty()) {
        p.setPen(QColor(240, 230, 140, 120));
        p.setFont(QFont("Arial", 32, QFont::Bold));
        p.drawText(r, Qt::AlignCenter, symbol);
    }
}

// ========================= МЫШЬ =========================
void SolitaireGame::mousePressEvent(QMouseEvent *event) {
    if (gameState != PLAYING) return;

    auto hit = getCardAt(event->x(), event->y());

    if (hit.type == -1) { resetSelection(); update(); return; }

    // Клик по колоде
    if (hit.type == 0) {
        if (!stock.empty()) {
            waste.push_back(stock.back());
            waste.back().faceUp = true;
            stock.pop_back();
        } else {
            while (!waste.empty()) {
                stock.push_back(waste.back());
                stock.back().faceUp = false;
                waste.pop_back();
            }
        }
        resetSelection();
        update();
        return;
    }

    // Попытка хода
    if (selType != -1) {
        if (hit.type == 2 || hit.type == 3)
            tryMove(hit.type, hit.pileIdx);
        else
            resetSelection();
        update();
        return;
    }

    // Выделение
    if (hit.type == 1 && hit.cardIdx == (int)waste.size() - 1)
        selectCard(1, 0, hit.cardIdx);
    else if (hit.type == 2 && hit.cardIdx >= 0 &&
             tableau[hit.pileIdx][hit.cardIdx].faceUp)
        selectCard(2, hit.pileIdx, hit.cardIdx);
    else if (hit.type == 3 && !foundations[hit.pileIdx].empty() &&
             hit.cardIdx == (int)foundations[hit.pileIdx].size() - 1)
        selectCard(3, hit.pileIdx, hit.cardIdx);

    update();
}

void SolitaireGame::mouseMoveEvent(QMouseEvent *event) {
    if (gameState != PLAYING) return;
    hoverPos = event->pos();
    update();
}

void SolitaireGame::leaveEvent(QEvent *) {
    hoverPos = {-1, -1};
    update();
}

// ========================= ХИТБОКСЫ =========================
SolitaireGame::HitResult SolitaireGame::getCardAt(int x, int y) {
    // ВАЖНО: проверяем в обратном порядке отрисовки, чтобы верхняя карта имела приоритет

    // 1. Tableau — идём с КОНЦА каждой стопки (верхние карты приоритетнее)
    for (int i = (int)tableau.size() - 1; i >= 0; --i) {
        int tx = tabStartX + i * (cardW + spacing);
        if (x < tx || x > tx + cardW) continue;

        if (tableau[i].empty()) {
            // Пустой слот — кликабельная область
            if (y >= tabY && y <= tabY + cardH)
                return {2, i, -1};
            continue;
        }

        // Идём с конца стопки — последняя отрисованная карта сверху
        int currentY = tabY;
        std::vector<int> ys;
        ys.reserve(tableau[i].size());
        for (int j = 0; j < (int)tableau[i].size(); ++j) {
            ys.push_back(currentY);
            currentY += tableau[i][j].faceUp ? tabOffset : tabOffsetDn;
        }

        // Проверяем с верхней карты вниз
        for (int j = (int)ys.size() - 1; j >= 0; --j) {
            if (y >= ys[j] && y <= ys[j] + cardH) {
                return {2, i, j};
            }
        }
    }

    // 2. Foundations
    for (int i = (int)foundations.size() - 1; i >= 0; --i) {
        int fx = foundStartX + i * (cardW + spacing);
        if (QRect(fx, foundY, cardW, cardH).contains(x, y))
            return {3, i, foundations[i].empty() ? -1 : (int)foundations[i].size() - 1};
    }

    // 3. Waste
    if (!waste.empty() && QRect(wasteX, foundY, cardW, cardH).contains(x, y))
        return {1, 0, (int)waste.size() - 1};

    // 4. Stock (самый нижний слой)
    if (QRect(stockX, foundY, cardW, cardH).contains(x, y))
        return {0, 0, 0};

    return {-1, -1, -1};
}

// ========================= СТАРТ / СБРОС =========================
void SolitaireGame::startGame(int decks) {
    resetGame();
    numDecks   = decks;
    gameState  = PLAYING;
    movesCount = 0;

    menuWidget->hide();
    btnStart->hide();
    topBar->show();
    updateMovesLabel();

    int tabSize = 5 + 2 * numDecks;
    foundStartX = width() - (4 * numDecks) * (cardW + spacing) - 20;

    std::vector<Card> deck;
    for (int d = 0; d < numDecks; ++d)
        for (int s = 0; s < 4; ++s)
            for (int r = 1; r <= 13; ++r)
                deck.push_back({s, r, false});

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);

    tableau.resize(tabSize);
    foundations.resize(4 * numDecks);

    int idx = 0;
    for (int i = 0; i < tabSize; ++i) {
        int cardsToDeal = (i < 7) ? (i + 1) : ((i - 7) % 2 + 1);
        for (int j = 0; j < cardsToDeal; ++j) {
            Card c = deck[idx++];
            if (j == cardsToDeal - 1) c.faceUp = true;
            tableau[i].push_back(c);
        }
    }
    while (idx < (int)deck.size())
        stock.push_back(deck[idx++]);

    update();
}

void SolitaireGame::resetGame() {
    stock.clear(); waste.clear();
    tableau.clear(); foundations.clear();
    resetSelection();
}

void SolitaireGame::backToMenu() {
    gameState = MENU;
    resetGame();
    topBar->hide();
    menuWidget->show();
    btnStart->hide();
    update();
}

// ========================= ВЫДЕЛЕНИЕ =========================
void SolitaireGame::selectCard(int type, int pile, int card) {
    selType = type; selPile = pile; selCard = card;
}
void SolitaireGame::resetSelection() {
    selType = selPile = selCard = -1;
}
bool SolitaireGame::isSelected(int type, int pile, int card) {
    if (selType == -1) return false;
    if (selType == 1 && type == 1) return true;
    if (selType == 3 && type == 3 && selPile == pile) return true;
    if (selType == 2 && type == 2 && selPile == pile && card >= selCard) return true;
    return false;
}

// ========================= ХОДЫ =========================
void SolitaireGame::tryMove(int destType, int destPile) {
    std::vector<Card> moving;
    if (selType == 1) moving = {waste.back()};
    else if (selType == 2)
        for (int i = selCard; i < (int)tableau[selPile].size(); ++i)
            moving.push_back(tableau[selPile][i]);
    else if (selType == 3)
        moving = {foundations[selPile].back()};

    if (moving.empty()) { resetSelection(); return; }

    bool valid = false;
    if (destType == 3) {
        if (moving.size() == 1) {
            if (foundations[destPile].empty())
                valid = (moving[0].rank == 1);
            else {
                Card &t = foundations[destPile].back();
                valid = (moving[0].rank == t.rank + 1 && moving[0].suit == t.suit);
            }
            if (valid) {
                foundations[destPile].push_back(moving[0]);
                removeSourceCards();
                movesCount++;
                updateMovesLabel();
                checkWin();
            }
        }
    } else if (destType == 2) {
        if (tableau[destPile].empty())
            valid = (moving[0].rank == 13);
        else {
            Card &t = tableau[destPile].back();
            valid = (moving[0].rank == t.rank - 1 && moving[0].color() != t.color());
        }
        if (valid) {
            for (auto &c : moving) tableau[destPile].push_back(c);
            removeSourceCards();
            movesCount++;
            updateMovesLabel();
        }
    }
    resetSelection();
}

void SolitaireGame::removeSourceCards() {
    if (selType == 1) {
        waste.pop_back();
    } else if (selType == 2) {
        tableau[selPile].resize(selCard);
        if (!tableau[selPile].empty() && !tableau[selPile].back().faceUp)
            tableau[selPile].back().faceUp = true;
    } else if (selType == 3) {
        foundations[selPile].pop_back();
    }
}

void SolitaireGame::checkWin() {
    int total = numDecks * 52, inFound = 0;
    for (auto &f : foundations) inFound += f.size();
    if (inFound == total) {
        QMessageBox::information(this, "🎉 Победа!",
            QString("Вы собрали пасьянс за %1 ходов!").arg(movesCount));
        backToMenu();
    }
}

void SolitaireGame::updateMovesLabel() {
    if (lblMoves) lblMoves->setText(QString("Ходов: %1").arg(movesCount));
}