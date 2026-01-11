#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

// Better cursor control in VS Code integrated terminal
#define RLUTIL_USE_ANSI
#include "rlutil.h"

using std::vector;

// ===== Mobile-like board size =====
static constexpr int W = 8;
static constexpr int H = 8;

struct Game {
    int board[H][W]{};          // 0 = empty, otherwise rlutil color id
    long long score = 0;
    long long best  = 0;

    struct Pt { int x, y; };

    struct Piece {
        vector<Pt> cells;       // relative coords
        int color = rlutil::WHITE;
        bool used = false;
    };

    std::mt19937 rng{ std::random_device{}() };
    std::array<Piece, 3> offers{};

    int selected = 0;
    int cursorX = 0;
    int cursorY = 0;

    int originX = 4;
    int originY = 2;

    bool firstDeal = true;

    static constexpr const char* BLOCK = "██";
    static constexpr const char* GRID  = "░░";

    // ---------- helpers ----------
    bool inBounds(int x, int y) const {
        return x >= 0 && x < W && y >= 0 && y < H;
    }

    void locateCell(int x, int y) const {
        // each cell width = 2 chars
        rlutil::locate(originX + 2 + x * 2, originY + 2 + y);
    }

    static void normalize(Piece& p) {
        int minx = 1e9, miny = 1e9;
        for (auto &c : p.cells) {
            minx = std::min(minx, c.x);
            miny = std::min(miny, c.y);
        }
        for (auto &c : p.cells) {
            c.x -= minx;
            c.y -= miny;
        }
        std::sort(p.cells.begin(), p.cells.end(),
                  [](const Pt& a, const Pt& b){
                      if (a.y != b.y) return a.y < b.y;
                      return a.x < b.x;
                  });
    }

    static Piece rotated90(const Piece& src) {
        // (x,y) -> (y,-x)
        Piece out = src;
        for (auto &c : out.cells) {
            int nx = c.y;
            int ny = -c.x;
            c.x = nx;
            c.y = ny;
        }
        normalize(out);
        return out;
    }

    // ---------- colors ----------
    int randomColor() {
        // allow repeats
        static const int COLORS[] = {
            rlutil::LIGHTCYAN,
            rlutil::LIGHTMAGENTA,
            rlutil::LIGHTGREEN,
            rlutil::YELLOW,
            rlutil::LIGHTBLUE
        };
        std::uniform_int_distribution<int> dist(0, (int)(sizeof(COLORS)/sizeof(COLORS[0])) - 1);
        return COLORS[dist(rng)];
    }

    // ---------- pieces ----------
    Piece makePiece(const vector<Pt>& cells, int color) {
        Piece p;
        p.cells = cells;
        p.color = color;
        p.used = false;
        normalize(p);
        return p;
    }

    Piece makeRect(int w, int h, int color) {
        vector<Pt> cells;
        cells.reserve(w * h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                cells.push_back({x, y});
        return makePiece(cells, color);
    }

    Piece randomPiece() {
        int color = randomColor();

        std::uniform_int_distribution<int> tDist(0, 11);
        int t = tDist(rng);

        switch (t) {
            case 0:  return makePiece({{0,0}}, color);
            case 1:  return makePiece({{0,0},{1,0}}, color);
            case 2:  return makePiece({{0,0},{0,1}}, color);
            case 3:  return makePiece({{0,0},{1,0},{2,0}}, color);
            case 4:  return makePiece({{0,0},{0,1},{0,2}}, color);
            case 5:  return makePiece({{0,0},{1,0},{0,1},{1,1}}, color);
            case 6:  return makePiece({{0,0},{1,0},{2,0},{3,0}}, color);
            case 7:  return makePiece({{0,0},{0,1},{0,2},{0,3}}, color);
            case 8:  return makePiece({{0,0},{0,1},{1,1}}, color);          // small L
            case 9:  return makePiece({{1,0},{0,1},{1,1}}, color);          // corner
            case 10: return makePiece({{0,0},{1,0},{2,0},{0,1}}, color);    // L4
            case 11: return makePiece({{0,0},{1,0},{2,0},{1,1}}, color);    // T-ish
            default: return makePiece({{0,0}}, color);
        }
    }

    void refillOffers() {
        for (int i = 0; i < 3; i++) offers[i].used = false;

        if (firstDeal) {
            // Opening: 3x3, 3x3, 2x3 (colors can repeat)
            offers[0] = makeRect(3, 3, randomColor());
            offers[1] = makeRect(3, 3, randomColor());
            offers[2] = makeRect(2, 3, randomColor());
            firstDeal = false;
        } else {
            for (int i = 0; i < 3; i++) {
                offers[i] = randomPiece();
                offers[i].used = false;
            }
        }

        selected = 0;
    }

    bool allUsed() const {
        return offers[0].used && offers[1].used && offers[2].used;
    }

    void rotateSelected() {
        if (offers[selected].used) return;
        offers[selected] = rotated90(offers[selected]);
    }

    void markUsedAndAutoSelect(int idx) {
        offers[idx].used = true;
        for (int k = 0; k < 3; k++) {
            int j = (idx + 1 + k) % 3;
            if (!offers[j].used) { selected = j; return; }
        }
    }

    // ---------- placement ----------
    bool canPlace(const Piece& p, int x, int y) const {
        for (auto &c : p.cells) {
            int bx = x + c.x;
            int by = y + c.y;
            if (!inBounds(bx, by)) return false;
            if (board[by][bx] != 0) return false;
        }
        return true;
    }

    void placePiece(const Piece& p, int x, int y) {
        for (auto &c : p.cells) {
            int bx = x + c.x;
            int by = y + c.y;
            board[by][bx] = p.color;
            score += 10;
        }
    }

    bool anyMovePossible() const {
        for (int i = 0; i < 3; i++) {
            if (offers[i].used) continue;
            const auto& p = offers[i];
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                    if (canPlace(p, x, y)) return true;
        }
        return false;
    }

    // ---------- clear lines ----------
    void flashLines(const vector<int>& rows, const vector<int>& cols) const {
        // flash with white blocks
        for (int t = 0; t < 2; t++) {
            rlutil::setColor(rlutil::WHITE);
            for (int r : rows) {
                for (int x = 0; x < W; x++) {
                    locateCell(x, r);
                    std::cout << BLOCK;
                }
            }
            for (int c : cols) {
                for (int y = 0; y < H; y++) {
                    locateCell(c, y);
                    std::cout << BLOCK;
                }
            }
            rlutil::resetColor();
            std::cout.flush();
            rlutil::msleep(60);

            // redraw board quickly
            drawBoardOnly();
            std::cout.flush();
            rlutil::msleep(60);
        }
    }

    int clearLines() {
        vector<int> fullRows, fullCols;

        for (int r = 0; r < H; r++) {
            bool full = true;
            for (int c = 0; c < W; c++) {
                if (board[r][c] == 0) { full = false; break; }
            }
            if (full) fullRows.push_back(r);
        }

        for (int c = 0; c < W; c++) {
            bool full = true;
            for (int r = 0; r < H; r++) {
                if (board[r][c] == 0) { full = false; break; }
            }
            if (full) fullCols.push_back(c);
        }

        if (fullRows.empty() && fullCols.empty()) return 0;

        flashLines(fullRows, fullCols);

        for (int r : fullRows)
            for (int c = 0; c < W; c++)
                board[r][c] = 0;

        for (int c : fullCols)
            for (int r = 0; r < H; r++)
                board[r][c] = 0;

        int cleared = (int)fullRows.size() + (int)fullCols.size();
        score += 120LL * cleared;
        return cleared;
    }

    // ---------- rendering ----------
    void drawFrame() const {
        rlutil::resetColor();
        rlutil::locate(originX, originY);
        rlutil::setColor(rlutil::WHITE);
        std::cout << "BLOCK BLAST (Console)";
        rlutil::resetColor();

        int left = originX;
        int top = originY + 1;
        int insideW = W * 2;
        int insideH = H;
        int right = left + 1 + insideW + 1;
        int bottom = top + insideH + 1;

        rlutil::setColor(rlutil::GREY);

        rlutil::locate(left, top);
        std::cout << "+";
        for (int i = 0; i < insideW; i++) std::cout << "-";
        std::cout << "+";

        for (int y = 1; y <= insideH; y++) {
            rlutil::locate(left, top + y);
            std::cout << "|";
            rlutil::locate(right, top + y);
            std::cout << "|";
        }

        rlutil::locate(left, bottom);
        std::cout << "+";
        for (int i = 0; i < insideW; i++) std::cout << "-";
        std::cout << "+";

        rlutil::resetColor();
    }

    void drawHUD() const {
        int hudX = originX + 2 + W * 2 + 6;
        int hudY = originY + 1;

        rlutil::locate(hudX, hudY);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "Score: " << score << "      ";
        rlutil::locate(hudX, hudY + 1);
        std::cout << "Best : " << best  << "      ";
        rlutil::resetColor();

        rlutil::locate(hudX, hudY + 3);
        rlutil::setColor(rlutil::LIGHTGREEN);
        std::cout << "Controls:";
        rlutil::resetColor();

        rlutil::locate(hudX, hudY + 4);
        std::cout << "1/2/3  Select piece";
        rlutil::locate(hudX, hudY + 5);
        std::cout << "Arrows Move cursor";
        rlutil::locate(hudX, hudY + 6);
        std::cout << "Enter  Place";
        rlutil::locate(hudX, hudY + 7);
        std::cout << "R      Rotate";
        rlutil::locate(hudX, hudY + 8);
        std::cout << "Esc    Quit";

        rlutil::locate(hudX, hudY + 10);
        rlutil::setColor(rlutil::CYAN);
        std::cout << "Selected: [" << (selected + 1) << "]   ";
        rlutil::resetColor();
    }

    // 8x8 grid: empty cells use grey "░░"
    void drawBoardOnly() const {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                locateCell(x, y);
                if (board[y][x] == 0) {
                    rlutil::setColor(rlutil::DARKGREY);
                    std::cout << GRID;
                    rlutil::resetColor();
                } else {
                    rlutil::setColor(board[y][x]);
                    std::cout << BLOCK;
                    rlutil::resetColor();
                }
            }
        }
    }

    void drawOffers() const {
        int baseX = originX;
        int baseY = originY + H + 4;

        rlutil::resetColor();
        rlutil::locate(baseX, baseY);
        std::cout << "Next pieces (press 1/2/3):";

        for (int i = 0; i < 3; i++) {
            int boxX = baseX + i * 18;
            int boxY = baseY + 1;

            // clear a small area
            for (int r = 0; r < 7; r++) {
                rlutil::locate(boxX, boxY + r);
                std::cout << "                ";
            }

            rlutil::locate(boxX, boxY);
            if (i == selected) {
                rlutil::setColor(rlutil::WHITE);
                std::cout << ">" << "[" << (i + 1) << "]";
                rlutil::resetColor();
            } else {
                rlutil::setColor(rlutil::GREY);
                std::cout << " " << "[" << (i + 1) << "]";
                rlutil::resetColor();
            }

            if (offers[i].used) {
                rlutil::setColor(rlutil::DARKGREY);
                rlutil::locate(boxX + 4, boxY + 2);
                std::cout << "(used)";
                rlutil::resetColor();
                continue;
            }

            int offX = boxX + 4;
            int offY = boxY + 1;

            rlutil::setColor(offers[i].color);
            for (auto &c : offers[i].cells) {
                rlutil::locate(offX + c.x * 2, offY + c.y);
                std::cout << BLOCK;
            }
            rlutil::resetColor();
        }
    }

    // Requirement: if NOT placeable, draw grey preview.
    // Important fix: do NOT paint grey over already-occupied colored blocks.
    void drawCursorAndPreview() const {
        if (offers[selected].used) {
            locateCell(cursorX, cursorY);
            rlutil::setColor(rlutil::WHITE);
            std::cout << BLOCK;
            rlutil::resetColor();
            return;
        }

        const auto& p = offers[selected];
        bool ok = canPlace(p, cursorX, cursorY);

        int previewColor = ok ? p.color : rlutil::DARKGREY;

        // draw preview ONLY on empty cells (so placed blocks won't turn grey)
        rlutil::setColor(previewColor);
        for (auto &c : p.cells) {
            int bx = cursorX + c.x;
            int by = cursorY + c.y;
            if (!inBounds(bx, by)) continue;

            if (board[by][bx] == 0) {
                locateCell(bx, by);
                std::cout << BLOCK;
            }
        }
        rlutil::resetColor();

        // cursor highlight
        locateCell(cursorX, cursorY);
        rlutil::setColor(ok ? rlutil::WHITE : rlutil::GREY);
        std::cout << BLOCK;
        rlutil::resetColor();
    }

    void fullRedraw() const {
        drawFrame();
        drawHUD();
        drawBoardOnly();
        drawOffers();
        drawCursorAndPreview();

        rlutil::locate(1, originY + H + 18);
        std::cout.flush();
    }

    // ---------- game over / reset ----------
    void gameOverScreen() {
        best = std::max(best, score);

        int gx = originX + 2;
        int gy = originY + 4;

        rlutil::locate(gx, gy);
        rlutil::setColor(rlutil::LIGHTRED);
        std::cout << "===== GAME OVER =====";
        rlutil::resetColor();

        rlutil::locate(gx, gy + 2);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "Score: " << score << "     ";
        rlutil::locate(gx, gy + 3);
        std::cout << "Best : " << best  << "     ";
        rlutil::resetColor();

        rlutil::locate(gx, gy + 5);
        std::cout << "Press any key to restart...";
        rlutil::anykey();
    }

    void resetGame() {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                board[y][x] = 0;

        score = 0;
        selected = 0;
        cursorX = 0;
        cursorY = 0;

        firstDeal = true;
        refillOffers();
    }

    // ---------- main loop ----------
    void run() {
        rlutil::cls();
        rlutil::saveDefaultColor();
        rlutil::hidecursor();

        refillOffers();

        bool quit = false;
        bool dirty = true;

        while (!quit) {
            best = std::max(best, score);

            if (!anyMovePossible()) {
                fullRedraw();
                gameOverScreen();
                resetGame();
                rlutil::cls();
                dirty = true;
                continue;
            }

            if (dirty) {
                fullRedraw();
                dirty = false;
            }

            if (kbhit()) { // Windows macro, NOT rlutil::kbhit()
                int key = rlutil::getkey();
                dirty = true;

                if (key == rlutil::KEY_ESCAPE) {
                    quit = true;
                    continue;
                }

                if (key == rlutil::KEY_UP) cursorY = std::max(0, cursorY - 1);
                else if (key == rlutil::KEY_DOWN) cursorY = std::min(H - 1, cursorY + 1);
                else if (key == rlutil::KEY_LEFT) cursorX = std::max(0, cursorX - 1);
                else if (key == rlutil::KEY_RIGHT) cursorX = std::min(W - 1, cursorX + 1);
                else if (key == rlutil::KEY_ENTER) {
                    if (!offers[selected].used && canPlace(offers[selected], cursorX, cursorY)) {
                        placePiece(offers[selected], cursorX, cursorY);
                        markUsedAndAutoSelect(selected);
                        clearLines();
                        if (allUsed()) refillOffers();
                    } else {
                        rlutil::msleep(20);
                    }
                } else {
                    if (key == '1') selected = 0;
                    else if (key == '2') selected = 1;
                    else if (key == '3') selected = 2;
                    else if (key == 'r' || key == 'R') rotateSelected();
                }
            } else {
                // faster response
                rlutil::msleep(1);
            }
        }

        rlutil::showcursor();
        rlutil::resetColor();
        rlutil::cls();
        std::cout << "Bye!\n";
    }
};

int main() {
#ifdef _WIN32
    // helps Unicode blocks in some terminals
    SetConsoleOutputCP(CP_UTF8);
#endif
    Game g;
    g.run();
    return 0;
}
