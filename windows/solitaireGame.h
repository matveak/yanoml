#ifndef SOLITAIREGAME_H
#define SOLITAIREGAME_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QPoint>
#include <vector>

struct Card {
    int suit;   // 0=♥, 1=♦, 2=♣, 3=♠
    int rank;   // 1..13
    bool faceUp;
    int color() const { return (suit == 0 || suit == 1) ? 0 : 1; }
};

class SolitaireGame : public QWidget {
    Q_OBJECT

public:
    explicit SolitaireGame(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // ===== UI =====
    QPushButton *btnStart;
    QPushButton *btnNewGame;
    QPushButton *btnBackToMenu;
    QLabel      *lblMoves;
    QWidget     *menuWidget;
    QWidget     *topBar;

    // ===== Константы отрисовки =====
    const int cardW       = 80;
    const int cardH       = 115;
    const int spacing     = 18;
    const int tabOffset   = 32;   // смещение открытых карт в столбце
    const int tabOffsetDn = 18;   // смещение закрытых карт
    const int topBarH     = 60;
    const int foundY      = 90;
    const int tabY        = 230;
    const int stockX      = 40;
    const int wasteX      = 135;

    int tabStartX   = 40;
    int foundStartX = 400;

    // ===== Состояние игры =====
    enum GameState { MENU, PLAYING };
    GameState gameState = MENU;
    int numDecks = 1;
    int movesCount = 0;

    std::vector<Card> stock, waste;
    std::vector<std::vector<Card>> tableau;
    std::vector<std::vector<Card>> foundations;

    // Выделение и hover
    int  selType = -1;   // 1=waste, 2=tab, 3=found
    int  selPile = -1;
    int  selCard = -1;
    QPoint hoverPos = {-1, -1};

    // ===== Методы =====
    void startGame(int decks);
    void resetGame();
    void backToMenu();

    void drawBackground(QPainter &p);
    void drawTopBar(QPainter &p);
    void drawCard(QPainter &p, int x, int y, const Card &c, bool selected, bool hovered);
    void drawEmptySlot(QPainter &p, int x, int y, const QString &symbol = "");

    struct HitResult { int type; int pileIdx; int cardIdx; };
    HitResult getCardAt(int x, int y);

    void selectCard(int type, int pile, int card);
    void resetSelection();
    bool isSelected(int type, int pile, int card);

    void tryMove(int destType, int destPile);
    void removeSourceCards();
    void checkWin();
    void updateMovesLabel();
};

#endif // SOLITAIREGAME_H