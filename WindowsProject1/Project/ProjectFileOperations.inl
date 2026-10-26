bool SaveProject(HWND hwnd)
{
    if (g_numericEdit) FinishNumericEdit(hwnd, true);
    if (g_isEditingFunction)
    {
        const bool saved = SaveCurrentFunction();
        if (saved)
            MessageBoxW(hwnd, L"関数を保存しました。", L"保存完了", MB_OK | MB_ICONINFORMATION);
        return saved;
    }
    wchar_t path[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    ofn.lpstrFilter =
        L"Block Editor Files (*.blk)\0*.blk\0"
        L"All Files (*.*)\0*.*\0";

    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"blk";

    ofn.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return false;

    if (!BlockFileManager::SaveToFile(
        path,
        ActiveManager().GetBlocks(),
        ActiveManager().GetParents()))
        return false;

    return SaveFunctionCallMetadata(path, ActiveManager().GetBlocks()) &&
        SaveRepeatBodyMetadata(path, ActiveManager().GetRepeatBodies());
}

// ============================================================
// プロジェクト読み込み
// ============================================================

bool LoadProject(HWND hwnd)
{
    if (g_numericEdit) FinishNumericEdit(hwnd, true);
    if (g_isEditingFunction)
    {
        MessageBoxW(hwnd, L"メインの「メイン」タブへ戻ってからプロジェクトを開いてください。関数の中身を誤って置き換えないための案内です。",
            L"プロジェクトを開く", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    wchar_t path[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    ofn.lpstrFilter =
        L"Block Editor Files (*.blk)\0*.blk\0"
        L"All Files (*.*)\0*.*\0";

    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"blk";

    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn))
        return false;

    std::vector<Block> loadedBlocks;
    std::vector<int> loadedParents;

    if (!BlockFileManager::LoadFromFile(
        path,
        loadedBlocks,
        loadedParents))
    {
        return false;
    }

    LoadFunctionCallMetadata(path, loadedBlocks);
    std::vector<int> loadedRepeatBodies;
    LoadRepeatBodyMetadata(path, loadedBlocks.size(), loadedRepeatBodies);

    if (!ActiveManager().ReplaceBlocks(
        loadedBlocks,
        loadedParents,
        loadedRepeatBodies))
    {
        return false;
    }

    InvalidateEditor();
    return true;
}


// ============================================================
// ブロック内の数値入力
// ============================================================

