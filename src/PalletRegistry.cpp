#include "PalletRegistry.h"

// ---------------------------------------------------------------------------
// Helper: convert inches to mm (round to nearest integer)
// Used only at registry population time so that all stored values are mm.
// ---------------------------------------------------------------------------
static int in2mm(double inches) {
    return static_cast<int>(inches * 25.4 + 0.5);
}

// ---------------------------------------------------------------------------
// Registry population
//
// Sources: docs/misc/standard-pallet-sizes.md (Ananno & Ribeiro project doc).
// Height note: H_pallet is the structural pallet height.
//              H_load defaults to Config::PALLET_H (1400 mm) — the algorithm's
//              cargo stacking zone, not the pallet's own height.
// For range heights (e.g. "~140–165 mm") the midpoint rounded to 5 mm is used;
// the comment records the source note.
// ---------------------------------------------------------------------------
static PalletRegistry::Registry buildRegistry() {
    PalletRegistry::Registry r;

    // Convenience lambda: insert a PalletSpec by enum ID.
    auto add = [&](PalletID id, PalletSpec ps) {
        r.emplace(id, std::move(ps));
    };

    // =======================================================================
    // EUROPEAN PALLETS — EPAL, CHEP, LPR, Craemer, CP chemical series
    // All natively mm.  Sources: EN 13698, EPAL, CHEP, LPR spec sheets.
    // =======================================================================

    // EPAL EUR 1 / Euro pallet — 800×1200 mm, mandated 144 mm (EN 13698-1)
    add(PalletID::EPAL_EUR_1,
        { .name="EPAL EUR 1 / Euro pallet",
          .L=1200, .W=800,  .H_pallet=144, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // CHEP Euro 1200×800 wood — same footprint, same spec height
    add(PalletID::CHEP_EURO_800x1200,
        { .name="CHEP Euro 1200×800 (wood)",
          .L=1200, .W=800,  .H_pallet=144, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // CHEP Euro 1200×800 plastic
    add(PalletID::CHEP_EURO_800x1200_P,
        { .name="CHEP Euro 1200×800 (plastic)",
          .L=1200, .W=800,  .H_pallet=160, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Craemer EURO L1 1200×800 plastic — 150 mm (mfr spec, craemer.com)
    add(PalletID::CRAEMER_EURO_L1,
        { .name="Craemer EURO L1 1200×800 (plastic)",
          .L=1200, .W=800,  .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Craemer NP1 1200×800 nestable plastic — 146 mm (mfr spec, craemer.com)
    add(PalletID::CRAEMER_NP1,
        { .name="Craemer NP1 1200×800 nestable (plastic)",
          .L=1200, .W=800,  .H_pallet=146, .max_weight=2500,
          .native_unit=Unit::Millimeters });

    // LPR PR080 Euro pallet — 144 mm (LPR spec sheet PDF)
    add(PalletID::LPR_PR080,
        { .name="LPR PR080 1200×800 (wood)",
          .L=1200, .W=800,  .H_pallet=144, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // GOST Euro 800×1200 — 145 mm (GOST 33757-2016, mandated standard)
    add(PalletID::GOST_EURO_800x1200,
        { .name="GOST Euro 800×1200 (wood)",
          .L=1200, .W=800,  .H_pallet=145, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // Turkey / North Africa EUR-compatible — ~144 mm (range; follows EUR norms)
    add(PalletID::EUR_COMPAT_800x1200,
        { .name="EUR-compatible 1200×800 (generic)",
          .L=1200, .W=800,  .H_pallet=144, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // EPAL EUR 2 / Industrial — 1000×1200 mm, 144 mm (EN 13698-2)
    add(PalletID::EPAL_EUR_2,
        { .name="EPAL EUR 2 / Industrial 1200×1000",
          .L=1200, .W=1000, .H_pallet=144, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // EPAL EUR 3 / Block industrial — same footprint as EUR 2, 144 mm
    add(PalletID::EPAL_EUR_3,
        { .name="EPAL EUR 3 / Block industrial 1200×1000",
          .L=1200, .W=1000, .H_pallet=144, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // CHEP UK 1200×1000 wood — 162 mm (CHEP B1210A spec sheet)
    add(PalletID::CHEP_UK_1200x1000,
        { .name="CHEP UK 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=162, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // CHEP UK 1200×1000 plastic — 160 mm (CHEP spec sheet)
    add(PalletID::CHEP_UK_1200x1000_P,
        { .name="CHEP UK 1200×1000 (plastic)",
          .L=1200, .W=1000, .H_pallet=160, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // LPR UK100 1200×1000 — 162 mm (retailer spec / Sketchfab 3D data)
    add(PalletID::LPR_UK100,
        { .name="LPR UK100 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=162, .max_weight=1200,
          .native_unit=Unit::Millimeters });

    // FIN Pallet 1000×1200 — 147 mm (Finnish mfr specs: me-wood.com)
    add(PalletID::FIN_1000x1200,
        { .name="Finnish Pallet 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=147, .max_weight=2500,
          .native_unit=Unit::Millimeters });

    // GOST FIN 1000×1200 — 145 mm (GOST 33757-2016, mandated)
    add(PalletID::GOST_FIN_1000x1200,
        { .name="GOST FIN 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=145, .max_weight=2000,
          .native_unit=Unit::Millimeters });

    // CHEP SA Code 8001 — 1000×1200, 166 mm (CHEP SA website)
    add(PalletID::CHEP_SA_CODE8001,
        { .name="CHEP SA Code 8001 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=166, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // South Africa general 1200×1000 wood — ~150 mm (range)
    add(PalletID::ZA_1200x1000,
        { .name="South Africa 1200×1000 (wood, generic)",
          .L=1200, .W=1000, .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // CHEP Mercosur / IRAM 10016 — 1000×1200, 145 mm (CHEP B1210C spec)
    add(PalletID::IRAM_MERCOSUR,
        { .name="IRAM 10016 Mercosur / CHEP Mercosur 1200×1000",
          .L=1200, .W=1000, .H_pallet=145, .max_weight=1800,
          .native_unit=Unit::Millimeters });

    // PBR1 Brazil — 1000×1200, ~141 mm wood (midpoint of 138–144 range)
    add(PalletID::PBR1_BRAZIL,
        { .name="PBR1 Brazil 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=141, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // PBR1 Brazil plastic — 150 mm (Arqplast/Tecnotri mfr spec)
    add(PalletID::PBR1_BRAZIL_P,
        { .name="PBR1 Brazil 1200×1000 (plastic)",
          .L=1200, .W=1000, .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Chile EUR-compatible 800×1200 — ~144 mm
    add(PalletID::CL_EUR_800x1200,
        { .name="Chile EUR-compatible 1200×800",
          .L=1200, .W=800,  .H_pallet=144, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Chile 1000×1200 — ~147 mm (midpoint of 144–150 range)
    add(PalletID::CL_1000x1200,
        { .name="Chile 1200×1000",
          .L=1200, .W=1000, .H_pallet=147, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Middle East GCC 1000×1200 plastic — 150 mm (mfr spec)
    add(PalletID::GCC_1200x1000_P,
        { .name="GCC Middle East 1200×1000 (plastic)",
          .L=1200, .W=1000, .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Middle East Saudi industrial 1200×1200 plastic — 150 mm (Alfuridi mfr)
    add(PalletID::SA_1200x1200_P,
        { .name="Saudi industrial 1200×1200 (plastic)",
          .L=1200, .W=1200, .H_pallet=150, .max_weight=3500,
          .native_unit=Unit::Millimeters });

    // EPAL EUR 6 / Half pallet (Düsseldorfer) — 800×600, 144 mm (EPAL standard)
    add(PalletID::EPAL_EUR_6,
        { .name="EPAL EUR 6 / Half pallet 800×600",
          .L=800,  .W=600,  .H_pallet=144, .max_weight=750,
          .native_unit=Unit::Millimeters });

    // CHEP Half 800×600 — 166 mm (CHEP B0806A spec; metal block construction)
    add(PalletID::CHEP_HALF_800x600,
        { .name="CHEP Half pallet 800×600 (wood+metal)",
          .L=800,  .W=600,  .H_pallet=166, .max_weight=400,
          .native_unit=Unit::Millimeters });

    // LPR DP610 600×1000 — 161 mm (LPR product listing)
    add(PalletID::LPR_DP610_600x1000,
        { .name="LPR DP610 600×1000 half beverage",
          .L=1000, .W=600,  .H_pallet=161, .max_weight=500,
          .native_unit=Unit::Millimeters });

    // EPAL EUR 7 / Quarter — 600×400, 144 mm (EPAL spec)
    add(PalletID::EPAL_EUR_7,
        { .name="EPAL EUR 7 / Quarter pallet 600×400",
          .L=600,  .W=400,  .H_pallet=144, .max_weight=500,
          .native_unit=Unit::Millimeters });

    // CHEP Quarter 600×400 plastic — 103 mm (CHEP P0604A spec)
    add(PalletID::CHEP_QUARTER_600x400_P,
        { .name="CHEP Quarter 600×400 (recycled PP)",
          .L=600,  .W=400,  .H_pallet=103, .max_weight=250,
          .native_unit=Unit::Millimeters });

    // CABKA Nest D2 600×400 — 140 mm (CABKA website)
    add(PalletID::CABKA_NEST_D2,
        { .name="CABKA Nest D2 600×400 display (HDPE)",
          .L=600,  .W=400,  .H_pallet=140, .max_weight=500,
          .native_unit=Unit::Millimeters });

    // -----------------------------------------------------------------------
    // EPAL CP Chemical pallets — two height tiers (138 mm: CP1–5; 156 mm: CP6–9)
    // Source: EPAL official CP specs (epal-pallets.org), Foresco confirmed.
    // -----------------------------------------------------------------------

    add(PalletID::CP1, { .name="CP1 chemical 1200×1000", .L=1200,.W=1000,.H_pallet=138,.native_unit=Unit::Millimeters });
    add(PalletID::CP2, { .name="CP2 chemical 1200×800",  .L=1200,.W=800, .H_pallet=138,.native_unit=Unit::Millimeters });
    add(PalletID::CP3, { .name="CP3 chemical 1140×1140", .L=1140,.W=1140,.H_pallet=138,.native_unit=Unit::Millimeters });
    add(PalletID::CP4, { .name="CP4 chemical 1300×1100", .L=1300,.W=1100,.H_pallet=138,.native_unit=Unit::Millimeters });
    add(PalletID::CP5, { .name="CP5 chemical 1140×760",  .L=1140,.W=760, .H_pallet=138,.native_unit=Unit::Millimeters });
    add(PalletID::CP6, { .name="CP6 chemical 1200×1000", .L=1200,.W=1000,.H_pallet=156,.native_unit=Unit::Millimeters });
    add(PalletID::CP7, { .name="CP7 chemical 1300×1100", .L=1300,.W=1100,.H_pallet=156,.native_unit=Unit::Millimeters });
    add(PalletID::CP8, { .name="CP8 chemical 1140×1140", .L=1140,.W=1140,.H_pallet=156,.native_unit=Unit::Millimeters });
    add(PalletID::CP9, { .name="CP9 chemical 1140×1140", .L=1140,.W=1140,.H_pallet=156,.native_unit=Unit::Millimeters });

    // =======================================================================
    // NORTH AMERICAN PALLETS — native unit: inches, stored in mm.
    // ISO 6780 rounds 48"×40" to 1219×1016 mm (not 1219.2×1016.0).
    // Heights from CHEP/PECO/Orbis spec sheets; ranges use midpoint rounded to 5 mm.
    // =======================================================================

    // GMA Stringer 48×40 — ~115 mm wood (midpoint of ~110–120 range)
    add(PalletID::NA_GMA_STRINGER_48x40,
        { .name="GMA Stringer 48\"×40\" (wood)",
          .L=1219, .W=1016, .H_pallet=115, .max_weight=907,
          .native_unit=Unit::Inches });

    // GMA Block 48×40 — ~155 mm wood (midpoint of 150–165 range)
    add(PalletID::NA_GMA_BLOCK_48x40,
        { .name="GMA Block 48\"×40\" (wood)",
          .L=1219, .W=1016, .H_pallet=155, .max_weight=1134,
          .native_unit=Unit::Inches });

    // CHEP Blue Block 48×40 — 141 mm exactly (CHEP spec sheet)
    add(PalletID::NA_CHEP_BLUE_48x40,
        { .name="CHEP Blue Block 48\"×40\"",
          .L=1219, .W=1016, .H_pallet=141, .max_weight=1270,
          .native_unit=Unit::Inches });

    // PECO Red Block 48×40 — ~140 mm (inferred; PECO spec PDF image-only)
    add(PalletID::NA_PECO_RED_48x40,
        { .name="PECO Red Block 48\"×40\"",
          .L=1219, .W=1016, .H_pallet=140, .max_weight=1270,
          .native_unit=Unit::Inches });

    // iGPS 48×40 plastic — ~152 mm (secondary sources; igps.net returned 403)
    add(PalletID::NA_IGPS_48x40,
        { .name="iGPS 48\"×40\" (HDPE plastic)",
          .L=1219, .W=1016, .H_pallet=152, .max_weight=1270,
          .native_unit=Unit::Inches });

    // Orbis Odyssey HD 48×40 plastic — 152 mm (orbiscorporation.com)
    add(PalletID::NA_ORBIS_HD_48x40,
        { .name="Orbis Odyssey HD 48\"×40\" (plastic)",
          .L=1219, .W=1016, .H_pallet=152, .max_weight=1270,
          .native_unit=Unit::Inches });

    // Orbis Odyssey LP 48×40 plastic — 142 mm (CHEP-height compatible)
    add(PalletID::NA_ORBIS_LP_48x40,
        { .name="Orbis Odyssey LP 48\"×40\" (plastic)",
          .L=1219, .W=1016, .H_pallet=142, .max_weight=1270,
          .native_unit=Unit::Inches });

    // Orbis HDSC 48×40 — 130 mm (custommhs.com distributor spec)
    add(PalletID::NA_ORBIS_HDSC_48x40,
        { .name="Orbis HDSC 48\"×40\" stackable (plastic)",
          .L=1219, .W=1016, .H_pallet=130, .max_weight=13608,
          .native_unit=Unit::Inches });

    // CABKA Retail US5 48×40 plastic — 148 mm (cabka.com)
    add(PalletID::NA_CABKA_US5_48x40,
        { .name="CABKA Retail US5 48\"×40\" (PE)",
          .L=1219, .W=1016, .H_pallet=148, .max_weight=1361,
          .native_unit=Unit::Inches });

    // CABKA Eco US5 48×40 recycled PP — 152 mm
    add(PalletID::NA_CABKA_ECO_48x40,
        { .name="CABKA Eco US5 48\"×40\" (recycled PP)",
          .L=1219, .W=1016, .H_pallet=152, .max_weight=3992,
          .native_unit=Unit::Inches });

    // Buckhorn Universal 48×40 — ~163 mm (distributor page; 175 mm with lip)
    add(PalletID::NA_BUCKHORN_48x40,
        { .name="Buckhorn Universal 48\"×40\" (HDPE)",
          .L=1219, .W=1016, .H_pallet=163, .max_weight=9072,
          .native_unit=Unit::Inches });

    // 48×48 Drum pallet — 1219×1219 mm, ~152 mm (midpoint of 140–165 range)
    add(PalletID::NA_48x48_DRUM,
        { .name="48\"×48\" Drum pallet (wood)",
          .L=in2mm(48), .W=in2mm(48), .H_pallet=152, .max_weight=1814,
          .native_unit=Unit::Inches });

    // 42×42 Telecom / Paint — ISO 4, 1067×1067 mm, ~130 mm (midpoint 115–152)
    add(PalletID::NA_42x42_TELECOM,
        { .name="42\"×42\" Telecom/Paint (ISO 4)",
          .L=1067, .W=1067, .H_pallet=130, .max_weight=907,
          .native_unit=Unit::Inches });

    // 48×45 Automotive AIAG — 1219×1143 mm, ~152 mm (midpoint 140–165)
    add(PalletID::NA_48x45_AUTO,
        { .name="48\"×45\" Automotive AIAG",
          .L=in2mm(48), .W=in2mm(45), .H_pallet=152, .max_weight=2268,
          .native_unit=Unit::Inches });

    // 48×42 Chemical/Beverage — 1219×1067 mm, ~140 mm (midpoint 127–152)
    add(PalletID::NA_48x42_CHEM,
        { .name="48\"×42\" Chemical/Beverage",
          .L=in2mm(48), .W=in2mm(42), .H_pallet=140, .max_weight=1134,
          .native_unit=Unit::Inches });

    // 44×44 Chemical/Drum — 1118×1118 mm, ~140 mm (midpoint 127–152)
    add(PalletID::NA_44x44_CHEM,
        { .name="44\"×44\" Chemical/Drum",
          .L=in2mm(44), .W=in2mm(44), .H_pallet=140, .max_weight=1134,
          .native_unit=Unit::Inches });

    // 40×40 Dairy — 1016×1016 mm, ~127 mm (midpoint 115–140)
    add(PalletID::NA_40x40_DAIRY,
        { .name="40\"×40\" Dairy",
          .L=in2mm(40), .W=in2mm(40), .H_pallet=127, .max_weight=1588,
          .native_unit=Unit::Inches });

    // 36×36 Beverage (Coke/Pepsi) — 914×914 mm, ~114 mm (midpoint 102–127)
    add(PalletID::NA_36x36_BEV,
        { .name="36\"×36\" Beverage (Coke/Pepsi)",
          .L=in2mm(36), .W=in2mm(36), .H_pallet=114, .max_weight=907,
          .native_unit=Unit::Inches });

    // 48×36 Paper/Roofing — 1219×914 mm, ~140 mm (midpoint 127–152)
    add(PalletID::NA_48x36_PAPER,
        { .name="48\"×36\" Paper/Roofing",
          .L=in2mm(48), .W=in2mm(36), .H_pallet=140, .max_weight=1134,
          .native_unit=Unit::Inches });

    // 48×20 Half retail — 1219×508 mm, ~140 mm (midpoint 127–152)
    add(PalletID::NA_48x20_HALF,
        { .name="48\"×20\" Retail half pallet",
          .L=in2mm(48), .W=in2mm(20), .H_pallet=140, .max_weight=635,
          .native_unit=Unit::Inches });

    // =======================================================================
    // ASIA-PACIFIC PALLETS
    // =======================================================================

    // T11 Japan (JIS Z 0601-2001) — 1100×1100, 144 mm (mandated)
    add(PalletID::JP_T11_JIS,
        { .name="T11 Japan JIS 1100×1100 (wood)",
          .L=1100, .W=1100, .H_pallet=144, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // JPR T11 plastic pool — 144 mm (JIS / kiefel.co.jp)
    add(PalletID::JP_T11_PLASTIC,
        { .name="JPR T11 1100×1100 (plastic)",
          .L=1100, .W=1100, .H_pallet=144, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // T11 Korea (KS T 1002) — 1100×1100, ~144 mm
    add(PalletID::KR_T11,
        { .name="T11 Korea KS 1100×1100 (wood)",
          .L=1100, .W=1100, .H_pallet=144, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // T12 Japan/Korea 1200×1000 — ~147 mm (midpoint 144–150)
    add(PalletID::JP_T12_1200x1000,
        { .name="T12 Japan/Korea 1200×1000",
          .L=1200, .W=1000, .H_pallet=147, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // China 1200×1000 wood — ~140 mm (midpoint 130–150; GB/T 2934 unregulated)
    add(PalletID::CN_1200x1000,
        { .name="China ECR Asia 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=140, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // China 1200×1000 plastic — 150 mm (mfr spec)
    add(PalletID::CN_1200x1000_P,
        { .name="China ECR Asia 1200×1000 (plastic)",
          .L=1200, .W=1000, .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // China 1100×1100 domestic — ~140 mm (midpoint 130–150)
    add(PalletID::CN_1100x1100,
        { .name="China domestic T11 1100×1100",
          .L=1100, .W=1100, .H_pallet=140, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // India 1200×1000 (CHEP India) — ~150 mm (midpoint 140–160)
    add(PalletID::IN_1200x1000,
        { .name="India CHEP 1200×1000",
          .L=1200, .W=1000, .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // India 1200×800 EUR-compatible — ~145 mm (midpoint 140–150)
    add(PalletID::IN_1200x800,
        { .name="India EUR-compatible 1200×800",
          .L=1200, .W=800,  .H_pallet=145, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // ASEAN 1200×1000 (Loscam SEA) — ~140 mm generic range
    add(PalletID::ASEAN_1200x1000,
        { .name="ASEAN Loscam SEA 1200×1000 (generic)",
          .L=1200, .W=1000, .H_pallet=140, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // Loscam SEA 1200×1000 — 156 mm exactly (Loscam spec sheet PDF)
    add(PalletID::LOSCAM_SEA_1200x1000,
        { .name="Loscam SEA 1200×1000 (wood)",
          .L=1200, .W=1000, .H_pallet=156, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // Singapore 1100×1100 Forest-Wood — 129 mm (Forest-Wood mfr spec)
    add(PalletID::SG_1100x1100,
        { .name="Singapore Forest-Wood 1100×1100",
          .L=1100, .W=1100, .H_pallet=129, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // Australia AS 4068 1165×1165 — 150 mm (AS 4068-1993 / CHEP + Loscam)
    add(PalletID::AU_AS4068_1165x1165,
        { .name="Australia AS 4068 1165×1165 (wood)",
          .L=1165, .W=1165, .H_pallet=150, .max_weight=2000,
          .native_unit=Unit::Millimeters });

    // CHEP AU 1165×1165 plastic — 150 mm (CHEP AU PP3 spec)
    add(PalletID::AU_CHEP_1165x1165_P,
        { .name="CHEP Australia 1165×1165 (plastic)",
          .L=1165, .W=1165, .H_pallet=150, .max_weight=2000,
          .native_unit=Unit::Millimeters });

    // Loscam AU 1165×1165 — 150 mm (Loscam PDF spec)
    add(PalletID::AU_LOSCAM_1165x1165,
        { .name="Loscam Australia 1165×1165 (wood)",
          .L=1165, .W=1165, .H_pallet=150, .max_weight=2000,
          .native_unit=Unit::Millimeters });

    // Australia export container 1100×1100 plastic — 120 mm (Palletwest mfr spec)
    add(PalletID::AU_EXPORT_1100x1100_P,
        { .name="Australia export container 1100×1100 (plastic)",
          .L=1100, .W=1100, .H_pallet=120, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // New Zealand 1200×1000 — ~150 mm (CHEP NZ / Loscam NZ inferred)
    add(PalletID::NZ_1200x1000,
        { .name="New Zealand CHEP 1200×1000",
          .L=1200, .W=1000, .H_pallet=150, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // Taiwan 1200×1000 (Loscam Greater China) — ~140 mm (midpoint 130–150)
    add(PalletID::TW_1200x1000,
        { .name="Taiwan Loscam 1200×1000",
          .L=1200, .W=1000, .H_pallet=140, .max_weight=1000,
          .native_unit=Unit::Millimeters });

    // PBR2 Brazil — 1050×1250, ~141 mm (midpoint 138–144)
    add(PalletID::PBR2_BRAZIL,
        { .name="PBR2 Brazil 1250×1050 (non-standard)",
          .L=1250, .W=1050, .H_pallet=141, .max_weight=1500,
          .native_unit=Unit::Millimeters });

    // South Africa Drum 1200×1200 — ~150 mm (range)
    add(PalletID::ZA_1200x1200_DRUM,
        { .name="SA Drum 1200×1200",
          .L=1200, .W=1200, .H_pallet=150, .max_weight=2000,
          .native_unit=Unit::Millimeters });

    return r;
}

// ---------------------------------------------------------------------------
// Singleton registry — built once at first call, never modified.
// ---------------------------------------------------------------------------
const PalletRegistry::Registry& PalletRegistry::get() {
    static const Registry registry = buildRegistry();
    return registry;
}

const PalletSpec& PalletRegistry::lookup(PalletID id) {
    const Registry& r = get();
    auto it = r.find(id);
    if (it == r.end()) {
        throw std::out_of_range(
            "PalletRegistry: PalletID " +
            std::to_string(static_cast<int>(id)) +
            " has no entry in the registry (missing add() call in PalletRegistry.cpp)");
    }
    return it->second;
}
