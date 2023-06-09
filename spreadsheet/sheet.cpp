#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <memory>

using namespace std::literals;

Sheet::~Sheet() {}

std::optional<std::string> Sheet::CheckCircularDependencies(Position pos,
                                                            const std::string &text) { // Создаем временную ячейку для проверки циклических зависимостей
    auto tmp_cell = std::make_unique<Cell>(*this);
    tmp_cell->Set(text);
    auto tmp_cell_ptr = tmp_cell.get();
// Проверяем на наличие циклических зависимостей
    if (tmp_cell->IsCyclicDependent(tmp_cell_ptr, pos)) {
// Возвращаем сообщение о циклической зависимости
        return "Circular dependency detected!";
    }

// Возвращаем пустой optional, если зависимостей нет
    return std::nullopt;
}

void Sheet::SetCell(Position pos,
                    std::string text) { // Невалидные позиции не обрабатываем if (!pos.IsValid()) { throw InvalidPositionException("Invalid position for SetCell()"); }
// Выделяем память под ячейку, если нужно
    Touch(pos);

// Получаем указатель на ячейку для текущего листа
    auto cell = GetCell(pos);

    if (cell) {
        // Ячейка уже существует.
        // Сохраняем старое содержимое на случай ввода некорректной формулы.
        std::string old_text = cell->GetText();

        // Инвалидируем кэш ячейки и зависимых от нее...
        InvalidateCell(pos);
        // ... и удаляем зависимости
        DeleteDependencies(pos);
        // Очищаем старое содержимое ячейки
        dynamic_cast<Cell *>(cell)->Clear();

        dynamic_cast<Cell *>(cell)->Set(text);
        // Проверяем на циклические зависимости новое содержимое cell
        auto circular_dependency_result = CheckCircularDependencies(pos, text);
        if (circular_dependency_result) {
            // Есть циклическая зависимость. Откат изменений
            dynamic_cast<Cell *>(cell)->Set(std::move(old_text));
            throw CircularDependencyException(*circular_dependency_result);
        }

        // Сохраняем зависимости
        for (const auto &ref_cell: dynamic_cast<Cell *>(cell)->GetReferencedCells()) {
            AddDependentCell(ref_cell, pos);
        }
    } else {
        // Новая ячейка (nullptr). Нужна проверка изменений Printable Area в конце
        auto new_cell = std::make_unique<Cell>(*this);
        new_cell->Set(text);

        // Проверяем циклические ссылки
        auto circular_dependency_result = CheckCircularDependencies(pos, text);
        if (circular_dependency_result) {
            throw CircularDependencyException(*circular_dependency_result);
        }

        // К настоящему моменту валидность формулы, позиции и отсутствие
        // циклических зависимостей проверены.
        // Переходим к модификации Sheet.

        // Проходим по вектору ячеек из формулы и добавляем
        // для каждой из них нашу ячейку как зависимую
        for (const auto &ref_cell: new_cell->GetReferencedCells()) {
            AddDependentCell(ref_cell, pos);
        }

        // Заменяем unique_ptr с nullptr из sheet_  на новый указатель
        sheet_.at(pos.row).at(pos.col) = std::move(new_cell);
        UpdatePrintableSize();
    }
}

const CellInterface *Sheet::GetCell(Position pos) const {
    // Невалидные позиции не обрабатываем
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position for GetCell()");
    }

    // т.к. не const GetCell() ничего не меняет в объекте Sheet,
    // вызываем ее с const-кастом текущего объекта
    // return const_cast<Sheet*>(this)->GetCell(pos);

    // Если память для ячейки выделена...
    if (CellExists(pos)) {
        //  ...и ее указатель не nullptr...
        if (sheet_.at(pos.row).at(pos.col)) {
            // ...возвращаем его
            return sheet_.at(pos.row).at(pos.col).get();
        }
    }

    // Для любых несуществующих ячеек возвращаем просто nullptr
    return nullptr;
}

CellInterface *Sheet::GetCell(Position pos) {
    // Невалидные позиции не обрабатываем
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position for GetCell()");
    }

    // Если память для ячейки выделена...
    if (CellExists(pos)) {
        //  ...и ее указатель не nullptr...
        if (sheet_.at(pos.row).at(pos.col)) {
            // ...возвращаем его
            return sheet_.at(pos.row).at(pos.col).get();
        }
    }

    // Для любых несуществующих ячеек возвращаем просто nullptr
    return nullptr;
}

void Sheet::ClearCell(Position pos) {
    // Невалидные позиции не обрабатываем
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position for ClearCell()");
    }

    if (CellExists(pos)) {
        //// Удаляем управляемый умным указателем ресурс, используя p.reset(p.get())
        //sheet_.at(pos.row).at(pos.col).reset(
        //    sheet_.at(pos.row).at(pos.col).get()
        //);
        sheet_.at(pos.row).at(pos.col).reset();       // Удаляет содержимое ячейки

        // pos.row/col 0-based          max_row/col 1-based
        if ((pos.row + 1 == max_row_) || (pos.col + 1 == max_col_)) {
            // Удаленная ячейка была на границе Printable Area. Нужен перерасчет
            area_is_valid_ = false;
            UpdatePrintableSize();
        }
    }
}

Size Sheet::GetPrintableSize() const {
    if (area_is_valid_) {
        return Size{max_row_, max_col_};
    }

    // Сюда попадать не должны. Можно либо вернуть {0,0}, либо бросить исключение

    // Во всех прочих случаях возвращаем {0, 0}
    //return { 0,0 };

    // Бросаем исключение
    throw InvalidPositionException("The size of printable area has not been updated");
}

void Sheet::PrintValues(std::ostream &output) const {
    for (int x = 0; x < max_row_; ++x) {
        bool need_separator = false;
        // Проходим по всей ширине Printable area
        for (int y = 0; y < max_col_; ++y) {
            // Проверка необходимости печати разделителя
            if (need_separator) {
                output << '\t';
            }
            need_separator = true;

            // Если мы не вышли за пределы вектора И ячейка не nullptr
            if ((y < static_cast<int>(sheet_.at(x).size())) && sheet_.at(x).at(y)) {
                // Ячейка существует
                auto value = sheet_.at(x).at(y)->GetValue();
                if (std::holds_alternative<std::string>(value)) {
                    output << std::get<std::string>(value);
                }
                if (std::holds_alternative<double>(value)) {
                    output << std::get<double>(value);
                }
                if (std::holds_alternative<FormulaError>(value)) {
                    output << std::get<FormulaError>(value);
                }
            }
        }
        // Разделение строк
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream &output) const {
    for (int x = 0; x < max_row_; ++x) {
        bool need_separator = false;
        // Проходим по всей ширине Printable area
        for (int y = 0; y < max_col_; ++y) {
            // Проверка необходимости печати разделителя
            if (need_separator) {
                output << '\t';
            }
            need_separator = true;

            // Если мы не вышли за пределы вектора И ячейка не nullptr
            if ((y < static_cast<int>(sheet_.at(x).size())) && sheet_.at(x).at(y)) {
                // Ячейка существует
                output << sheet_.at(x).at(y)->GetText();
            }
        }
        // Разделение строк
        output << '\n';
    }
}

void Sheet::InvalidateCell(const Position &pos) {
    // Для всех зависимых ячеек рекурсивно инвалидируем кэш
    for (const auto &dependent_cell: GetDependentCells(pos)) {
        auto cell = GetCell(dependent_cell);
        // InvalidateCache() есть только у Cell, приводим указатель
        dynamic_cast<Cell *>(cell)->InvalidateCache();
        InvalidateCell(dependent_cell);
    }
}

void Sheet::AddDependentCell(const Position &main_cell, const Position &dependent_cell) {
    // При отсутствии записи для main_cell создаем ее через []
    cells_dependencies_[main_cell].insert(dependent_cell);
}

const std::set<Position> Sheet::GetDependentCells(const Position &pos) {
    if (cells_dependencies_.count(pos) != 0) {
        // Есть такой ключ в словаре зависимостей. Возвращаем значение
        return cells_dependencies_.at(pos);
    }

    // Если мы здесь, от ячейки pos никто не зависит
    return {};
}

void Sheet::DeleteDependencies(const Position &pos) {
    cells_dependencies_.erase(pos);
}

void Sheet::UpdatePrintableSize() {
    max_row_ = 0;
    max_col_ = 0;

    // Сканируем ячейки, пропуская nullptr
    for (int x = 0; x < static_cast<int>(sheet_.size()); ++x) {
        for (int y = 0; y < static_cast<int>(sheet_.at(x).size()); ++y) {
            if (sheet_.at(x).at(y)) {
//                max_row_ = (max_row_ < (x + 1) ? x + 1 : max_row_);
//                max_col_ = (max_col_ < (y + 1) ? y + 1 : max_col_);
                max_row_ = std::max(max_row_, x + 1);
                max_col_ = std::max(max_col_, y + 1);
            }
        }
    }

    // Перерасчет произведен
    area_is_valid_ = true;
}

bool Sheet::CellExists(Position pos) const {
    return (pos.row < static_cast<int>(sheet_.size())) && (pos.col < static_cast<int>(sheet_.at(pos.row).size()));
}

void Sheet::Touch(Position pos) {
    // Невалидные позиции не обрабатываем
    if (!pos.IsValid()) {
        return;
    }

    // size() 1-based          pos.row/col 0-based          sheet_[] 0-based

    // Если элементов в векторе строк меньше, чем номер строки в pos.row...
    if (static_cast<int>(sheet_.size()) < (pos.row + 1)) {
        // ... резервируем и инициализируем nullptr элементы вплоть до строки pos.row
        sheet_.reserve(pos.row + 1);
        sheet_.resize(pos.row + 1);
    }

    // Если элементов в векторе столбцов меньше, чем номер столбца в pos.col...
    if (static_cast<int>(sheet_.at(pos.row).size()) < (pos.col + 1)) {
        // ... резервируем и инициализируем nullptr элементы вплоть до столбца pos.col
        sheet_.at(pos.row).reserve(pos.col + 1);
        sheet_.at(pos.row).resize(pos.col + 1);
    }
}


// Создаёт готовую к работе пустую таблицу. Объявление в common.h
std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}