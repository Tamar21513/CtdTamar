# בדיקות doctest ו-Test Coverage

## מה נוסף

הבדיקות נמצאות ב-`tests/doctest` ומחולקות לפי אחריות:

- `test_position_piece.cpp` — Position ו-Piece.
- `test_board_parser_mapper.cpp` — Board, parser, printer ומיפוי פיקסלים.
- `test_rules.cpp` — כל חוקי התנועה ו-RuleEngine.
- `test_engine_controller.cpp` — תזמון, תנועות מקבילות, התנגשויות, קפיצות, הכתרה ו-Controller.
- `test_realtime_arbiter.cpp` — אירועי זמן של RealTimeArbiter.

`test_main.cpp` מגדיר את נקודת הכניסה של doctest. אין לצרף את `main.cpp` הרגיל לקומפילציית הבדיקות.

## הרצה ב-Windows

1. התקיני Visual Studio 2022 או Build Tools עם הרכיב **Desktop development with C++**.
2. פתחי מתפריט התחל: **x64 Native Tools Command Prompt for VS 2022**.
3. עברי לתיקיית הפרויקט:

```bat
cd "C:\path\to\CTD"
```

4. הריצי:

```bat
tests\run_doctest.bat
```

להצגת בדיקות שנכשלו בלבד אפשר להריץ לאחר הקומפילציה:

```bat
doctest_tests.exe
```

להצגת כל assertion שעבר:

```bat
doctest_tests.exe --success
```

להרצת קבוצה מסוימת בלבד:

```bat
doctest_tests.exe --test-case="*Pawn*"
doctest_tests.exe --test-suite="Controller"
```

## הרצה ב-Linux / MinGW עם g++

```bash
chmod +x tests/run_doctest.sh
./tests/run_doctest.sh
```

## התקנת OpenCppCoverage ב-Windows

1. הורידי והתקיני **OpenCppCoverage**.
2. ודאי שהקובץ קיים בנתיב:

```text
C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe
```

3. פתחי שוב **x64 Native Tools Command Prompt for VS 2022**.
4. מתוך תיקיית הפרויקט הריצי:

```bat
tests\run_coverage.bat
```

הסקריפט מקמפל עם `/Zi /Od`, מריץ את כל הבדיקות ויוצר:

```text
coverage-report\index.html
```

הדוח נפתח אוטומטית. אם לא, פתחי ידנית:

```bat
start coverage-report\index.html
```

## פירוש הדוח

- שורה מכוסה: לפחות בדיקה אחת הפעילה אותה.
- שורה לא מכוסה: צריך להוסיף מצב שמגיע אליה.
- 100% Line Coverage אינו מוכיח שהקוד נכון; צריך גם לבדוק תוצאות נכונות ושני צדדים של תנאים.

## הערות

- `include/doctest.h` כבר נמצא בפרויקט.
- קובצי הבדיקה אינם משנים את קוד המשחק.
- הבדיקות כוללות גבולות של `999/1000` ו-`1999/2000` מילישניות, גבולות פיקסלים, לוח ריק, tokens שגויים, תנועה מקבילה, התנגשות, קפיצה והכתרת רגלי.
