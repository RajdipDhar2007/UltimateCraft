"""Material colour tables shared by the block, item and mob generators."""

DYES = ["white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray", "cyan",
        "purple", "blue", "brown", "green", "red", "black"]
DYE_RGB = {"white": 0xF0F3F3, "orange": 0xF07613, "magenta": 0xBD44B3, "light_blue": 0x3AAFD9, "yellow": 0xF8C527,
           "lime": 0x70B919, "pink": 0xED8DAC, "gray": 0x3E4447, "light_gray": 0x8E8E86, "cyan": 0x158991,
           "purple": 0x792AAC, "blue": 0x35399D, "brown": 0x724728, "green": 0x546D1B, "red": 0xA12722, "black": 0x141519}
TERRACOTTA_RGB = {"white": 0xD1B2A1, "orange": 0xA15325, "magenta": 0x95576C, "light_blue": 0x706C8A, "yellow": 0xBA8523,
                  "lime": 0x677535, "pink": 0xA14E4E, "gray": 0x392A23, "light_gray": 0x876A61, "cyan": 0x565B5B,
                  "purple": 0x764656, "blue": 0x4A3B5B, "brown": 0x4D3323, "green": 0x4C532A, "red": 0x8E3C2E, "black": 0x251610}

WOODS = ["oak", "spruce", "birch", "jungle", "acacia", "cherry", "dark_oak", "pale_oak", "mangrove", "bamboo", "crimson", "warped"]
PLANKS_RGB = {"oak": 0xB8945F, "spruce": 0x7A5A3A, "birch": 0xD7C185, "jungle": 0xB88764, "acacia": 0xBA6337,
              "cherry": 0xE3B3AD, "dark_oak": 0x4F3218, "pale_oak": 0xE6DCD6, "mangrove": 0x7F3632, "bamboo": 0xC9B551,
              "crimson": 0x6B344B, "warped": 0x2B6963}
BARK_RGB = {"oak": 0x6D5532, "spruce": 0x3B2712, "birch": 0xE3E0D6, "jungle": 0x584419, "acacia": 0x676157,
            "cherry": 0x3A1B21, "dark_oak": 0x3E2F18, "pale_oak": 0x6F6560, "mangrove": 0x543A26, "bamboo": 0x7A8A2A,
            "crimson": 0x5C1D1D, "warped": 0x3A3A55}
STRIPPED_RGB = {"oak": 0xB29157, "spruce": 0x735A3A, "birch": 0xC4B07A, "jungle": 0xAB844F, "acacia": 0xAE5C3B,
                "cherry": 0xD69A92, "dark_oak": 0x5F4A2B, "pale_oak": 0xE0D5CF, "mangrove": 0x773731, "bamboo": 0xC0AE4F,
                "crimson": 0x893A57, "warped": 0x3A8A80}
LEAVES_RGB = {"oak": 0x9a9a9a, "spruce": 0x619961, "birch": 0x80A755, "jungle": 0x9a9a9a, "acacia": 0x9a9a9a,
              "cherry": 0xF1B7CF, "dark_oak": 0x9a9a9a, "pale_oak": 0x9DA197, "mangrove": 0x9a9a9a,
              "azalea": 0x5E7C2A, "flowering_azalea": 0x5E7C2A}
LEAVES_TINT = {"oak": "foliage", "jungle": "foliage", "acacia": "foliage", "dark_oak": "foliage", "mangrove": "foliage"}

COPPER_STAGES = ["", "exposed_", "weathered_", "oxidized_"]
COPPER_RGB = {"": 0xC46A4C, "exposed_": 0xA27E68, "weathered_": 0x6C9A6E, "oxidized_": 0x52A386}

# full-block materials referenced by stairs/slabs/walls
STONE_RGB = {
    "stone": 0x7D7D7D, "smooth_stone": 0x9E9E9E, "cobblestone": 0x7A7A7A, "mossy_cobblestone": 0x6E7A5E,
    "stone_bricks": 0x7A7A7A, "mossy_stone_bricks": 0x6E7A5E, "cracked_stone_bricks": 0x767676, "chiseled_stone_bricks": 0x777777,
    "granite": 0x9A6B57, "polished_granite": 0x9F6C58, "diorite": 0xBCBCBC, "polished_diorite": 0xC1C1C3,
    "andesite": 0x888889, "polished_andesite": 0x848686, "deepslate": 0x505052, "cobbled_deepslate": 0x4D4D50,
    "polished_deepslate": 0x484849, "deepslate_bricks": 0x464646, "cracked_deepslate_bricks": 0x434343,
    "deepslate_tiles": 0x373737, "cracked_deepslate_tiles": 0x353535, "chiseled_deepslate": 0x363637,
    "calcite": 0xDFE0DC, "tuff": 0x6C6D66, "polished_tuff": 0x626A62, "tuff_bricks": 0x62665E, "chiseled_tuff": 0x5E6058,
    "chiseled_tuff_bricks": 0x63675F, "dripstone_block": 0x866B5C,
    "bricks": 0x966253, "mud_bricks": 0x89684F, "packed_mud": 0x8E6B50,
    "sandstone": 0xD8CB9B, "smooth_sandstone": 0xE0D6AA, "cut_sandstone": 0xD9CE9F, "chiseled_sandstone": 0xD8CA9B,
    "red_sandstone": 0xBA6621, "smooth_red_sandstone": 0xB5621F, "cut_red_sandstone": 0xBD6A22, "chiseled_red_sandstone": 0xB7601F,
    "prismarine": 0x63A39A, "prismarine_bricks": 0x63AB9E, "dark_prismarine": 0x335B4B,
    "nether_bricks": 0x2C1519, "red_nether_bricks": 0x450709, "cracked_nether_bricks": 0x281416, "chiseled_nether_bricks": 0x2F171B,
    "quartz_block": 0xEBE5DE, "smooth_quartz": 0xEDE7E0, "quartz_bricks": 0xEAE5DD, "chiseled_quartz_block": 0xE7E2DA, "quartz_pillar": 0xECE6DF,
    "purpur_block": 0xA97DA9, "purpur_pillar": 0xAB81AB,
    "end_stone": 0xDBDE9E, "end_stone_bricks": 0xDAE0A2,
    "blackstone": 0x2A2328, "polished_blackstone": 0x35303A, "polished_blackstone_bricks": 0x302B32,
    "cracked_polished_blackstone_bricks": 0x2C2730, "chiseled_polished_blackstone": 0x35303A, "gilded_blackstone": 0x382B26,
    "resin_bricks": 0xCE5A1C, "chiseled_resin_bricks": 0xC9561A, "resin_block": 0xD9631B,
    "netherrack": 0x6F3535, "basalt": 0x505055, "polished_basalt": 0x636367, "smooth_basalt": 0x48484E,
    "obsidian": 0x0F0B19, "crying_obsidian": 0x200A3A, "bedrock": 0x555555, "reinforced_deepslate": 0x505353,
    "terracotta": 0x985E43, "sculk": 0x0B2A33, "magma_block": 0x8E3F1F,
}

def tier_rgb(t):
    return {"wooden": 0x9A7A4A, "stone": 0x8A8A8A, "copper": 0xD8875F, "iron": 0xD8D8D8, "golden": 0xF4D33F,
            "diamond": 0x4AEDD9, "netherite": 0x4A4245, "divine": 0xF6E7A8, "leather": 0x9C5A35, "chainmail": 0x9A9A9A, "turtle": 0x47A036}[t]
