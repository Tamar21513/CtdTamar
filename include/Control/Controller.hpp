#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <optional>

#include "../Engine/GameEngine.hpp"
#include "../IO/BoardMapper.hpp"
#include "../Core/Position.hpp"
#include "../Core/Results.hpp"

using namespace std;

class Controller {
private:
    GameEngine& engine;

    optional<Position> selectedCell;

    int boardStartX;
    int boardStartY;

    int cellSizeX;
    int cellSizeY;

    optional<Position> mapClickToCell(
        int x,
        int y
    ) const;

public:
    /*
     * עבור הקונסול והטסטים:
     * הלוח מתחיל ב־0,0 ותא הוא 100×100.
     */
    Controller(GameEngine& engine);

    /*
     * עבור VisualApp:
     * מקבלים את המיקום והגודל האמיתיים
     * של הלוח בחלון.
     */
    Controller(
        GameEngine& engine,
        int boardStartX,
        int boardStartY,
        int cellSizeX,
        int cellSizeY
    );

    /*
     * כל לחיצת עכבר רגילה מגיעה לכאן.
     *
     * ה־Controller מחליט אם זו:
     * - בחירה
     * - החלפת בחירה
     * - תנועה
     * - קפיצה
     * - ביטול בחירה
     */
    ControllerResult click(
        int x,
        int y
    );

    /*
     * נשאר עבור פקודת jump המפורשת
     * שקיימת בקונסול.
     */
    ControllerResult jump(
        int x,
        int y
    );

    bool hasSelection() const;

    optional<Position>
    getSelectedCell() const;
};

#endif