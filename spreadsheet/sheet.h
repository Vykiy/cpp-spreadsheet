#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <map>
#include <set>

class Sheet : public SheetInterface
{
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    // ������� unique_ptr ������ � �������� (������ � ����������)
    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    // ������ ������� ��� ������ � ���������� �������� (����, ������� 
    // ������� �� ��������).
    // �������� � Sheet, �.�.:
    // 1) ��� ������ ����������� ������ Cell
    // 2) ������ � ��������� ������� �� ��������� � Cell, ��� "�������" ������
    // 3) ����� �������� ������ �������� ��������� �����

    // ���������� ����� ���� ��� ��������� ������ � ���� ��������� �� ���
    void InvalidateCell(const Position& pos);
    // ��������� ����������� "�������� ������" - "��������� ������".
    // dependent_cell ���� ����� == this
    void AddDependentCell(const Position& main_cell, const Position& dependent_cell);
    // ���������� �������� �����, ��������� �� pos
    const std::set<Position> GetDependentCells(const Position& pos);
    // ������� ��� ����������� ��� ������ pos.
    void DeleteDependencies(const Position& pos);

private:
    // ������ ��� ����� ����� ������� ��������� ����� (������ - ������ ��������� �� ���)
    std::map<Position, std::set<Position>> cells_dependencies_;

    std::vector<std::vector<std::unique_ptr<Cell>>> sheet_;
    /* ��� ����������� � ������� �������� ��� ����� (��� ����� ������� ���������)
    *  1) ����� ������� �������, ������������ ����������
    *  2) ������������ � �������� ��������� ����� ���������� � �������� ����������
    *  3) ����� ������ ����������� ������� �������� �������������� ����� �������
    *     ������ ��������� �� �������� ��� ������������ �����.
    *  4) �� ����� ������������ ������������ ����� open office, ������� ������ �� ������
    *     ���������� � ����������� ����������� ������ ��� �������� ��������� ������� 
    *     �� ��������� �������� ��������. ��������, ����������� ������ � Excel ��������������, 
    *     � � open offece - ���, ������ ������ ������� ��������. ���, ��� ���, 
    *     �� ������ ������� ���� ����������� ��������.
    */

    int max_row_ = 0;    // ����� ����� � Printable Area
    int max_col_ = 0;    // ����� �������� � Printable Area
    bool area_is_valid_ = true;    // ���� ���������� ������� �������� max_row_/col_

    // ������������ ������������ ������ ������� ������ �����
    void UpdatePrintableSize();
    // ��������� ������������� ������ �� ����������� pos, ���� ��� ����� ���� nullptr
    bool CellExists(Position pos) const;
    // ����������� ������ ��� ������, ���� ������ � ����� ������� �� ����������.
    // �� ���������� ���������������� �������� ��������� � �� ������������ Printale Area
    void Touch(Position pos);
};