void UpdateView()
{
    g_zoom = (std::max)(
        MIN_ZOOM,
        (std::min)(g_zoom, MAX_ZOOM)
        );

    ActiveManager().SetViewTransform(
        g_zoom,
        g_viewX,
        g_viewY
    );
}

// ============================================================
// カテゴリ別のブロック一覧（前方宣言）
// ============================================================

std::vector<BlockType> GetBlockTypes(BlockCategory category);

BlockCategory GetCategoryFromBlockType(BlockType type)
{
    for (int i = 0; i < CATEGORY_COUNT; ++i)
    {
        BlockCategory category =
            static_cast<BlockCategory>(i);

        std::vector<BlockType> types =
            GetBlockTypes(category);

        for (BlockType t : types)
        {
            if (t == type)
                return category;
        }
    }

    return BlockCategory::Other;
}
// ============================================================
// カテゴリ別のブロック一覧
// ============================================================

