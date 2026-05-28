*******************************************************************************
* Phase 13 - CoPico X-BIOS v1.0
* Toggle marker $01 in strings switches between normal/inverse rendering
*******************************************************************************

POLCAT      EQU     $A000
CHROUT      EQU     $A002

VAR_AUDIO   EQU     $0100
VAR_VIDEO   EQU     $0101
VAR_DISK    EQU     $0102
VAR_COMM    EQU     $0103
VAR_RTC     EQU     $0104
VAR_TMODE   EQU     $0105       * Toggle mode for PSTR_T ($40=normal,$00=inv)
VAR_SAVE    EQU     $0106       * RP2350 save trigger (write SAVE_MAGIC)
SAVE_MAGIC  EQU     $A5

STATUS_BUF  EQU     $D800
STATUS_FW   EQU     $D820
STATUS_SD   EQU     $D840
STATUS_TZ   EQU     $D860
STATUS_FLASH EQU    $D880
STATUS_RESULT EQU   $D881
REG_AUDIO   EQU     $FF70
REG_VIDEO   EQU     $FF71
REG_DISK    EQU     $FF72
REG_COMM    EQU     $FF73
REG_RTC     EQU     $FF74
REG_TZ      EQU     $FF75
REG_FLASH_CMD EQU   $FF76
REG_SAVE_STATUS EQU $FF7E
REG_BOOT    EQU     $FF7F
FLASH_CMD_SCAN    EQU 1
FLASH_CMD_CLEAR   EQU 2
FLASH_CMD_FACTORY EQU 3
FLASH_CMD_INTERNAL EQU 9
SAVE_STATUS_IDLE    EQU 0
SAVE_STATUS_PENDING EQU 1
SAVE_STATUS_OK      EQU 2
SAVE_STATUS_ERR     EQU 3
RESULT_BUSY       EQU 1
RESULT_OK         EQU 2
RESULT_ERR        EQU 3

            ORG     $C000
            FCC     "DK"
            FDB     START

START:
            LDS     #$03FF
            * Preload VAR_* from RP2350 saved-config registers. The 6809
            * stores them itself so $0100-$0104 init happens in the same bus
            * context as later toggling (consistent SAM page).
            LDA     REG_AUDIO
            STA     VAR_AUDIO
            LDA     REG_VIDEO
            STA     VAR_VIDEO
            LDA     REG_DISK
            STA     VAR_DISK
            LDA     REG_COMM
            STA     VAR_COMM
            LDA     REG_RTC
            STA     VAR_RTC

            JSR     CHECK_SETUP

MAIN_LOOP:
            LBSR    DRAW_MAIN

INPUT_LOOP:
            JSR     [POLCAT]
            TSTA
            BEQ     INPUT_LOOP
            CMPA    #'A'
            LBEQ    DO_AUDIO
            CMPA    #'a'
            LBEQ    DO_AUDIO
            CMPA    #'V'
            LBEQ    DO_VIDEO
            CMPA    #'v'
            LBEQ    DO_VIDEO
            CMPA    #'D'
            LBEQ    DO_DISK
            CMPA    #'d'
            LBEQ    DO_DISK
            CMPA    #'C'
            LBEQ    DO_COMM
            CMPA    #'c'
            LBEQ    DO_COMM
            CMPA    #'R'
            LBEQ    DO_RTC
            CMPA    #'r'
            LBEQ    DO_RTC
            CMPA    #'O'
            LBEQ    DO_OPTIONS
            CMPA    #'o'
            LBEQ    DO_OPTIONS
            CMPA    #'I'
            LBEQ    DO_INFO
            CMPA    #'i'
            LBEQ    DO_INFO
            CMPA    #13
            LBEQ    DO_BOOT
            BRA     INPUT_LOOP

DO_AUDIO:   LDA     VAR_AUDIO
            INCA
            CMPA    #3
            BNE     DA_W0
            CLRA
DA_W0:      STA     VAR_AUDIO
            * Orch90 ($FF7A/B) conflicts with WordPak V9958 on same ports — skip it
            LDA     VAR_VIDEO
            CMPA    #0
            BNE     DA_DONE
            LDA     VAR_AUDIO
            CMPA    #1
            BNE     DA_DONE
            LDA     #2
            STA     VAR_AUDIO
DA_DONE:    LBRA    MAIN_LOOP

DO_VIDEO:   LDA     VAR_VIDEO
            INCA
            CMPA    #3
            BNE     DV_W0
            CLRA
DV_W0:      STA     VAR_VIDEO
            * Selecting WordPak auto-disables Orch90 (shared $FF7A/$FF7B)
            LDA     VAR_VIDEO
            CMPA    #0
            BNE     DV_DONE
            LDA     VAR_AUDIO
            CMPA    #1
            BNE     DV_DONE
            LDA     #2
            STA     VAR_AUDIO
DV_DONE:    LBRA    MAIN_LOOP

DO_DISK:    LDA     VAR_DISK
            INCA
            CMPA    #4
            BNE     DD_SAVE
            CLRA
DD_SAVE:    STA     VAR_DISK
            CMPA    #1          * FujiNet selected?
            BNE     DD_DONE
            LDA     #2          * Force Comm to FujiNet
            STA     VAR_COMM
DD_DONE:    LBRA    MAIN_LOOP

DO_COMM:    LDA     VAR_COMM
            INCA
            CMPA    #4
            BNE     DC_SAVE
            CLRA
DC_SAVE:    STA     VAR_COMM
            CMPA    #2          * FujiNet selected?
            BNE     DC_DONE
            LDA     #1          * Force Disk to FujiNet
            STA     VAR_DISK
DC_DONE:    LBRA    MAIN_LOOP
DO_RTC:     LDA     VAR_RTC
            EORA    #$01
            STA     VAR_RTC
            LBRA    MAIN_LOOP
DO_OPTIONS:
            LBRA    OPTIONS_LOOP
DO_INFO:
            LBRA    LOOP_INFO
DO_BOOT:
            LDA     VAR_AUDIO
            STA     REG_AUDIO
            LDA     VAR_VIDEO
            STA     REG_VIDEO
            LDA     VAR_DISK
            STA     REG_DISK
            LDA     VAR_COMM
            STA     REG_COMM
            LDA     VAR_RTC
            STA     REG_RTC
            LBSR    DRAW_SAVING
            LBSR    WAIT_ANY_KEY
            LDA     #SAVE_MAGIC
            STA     VAR_SAVE
            LBSR    DELAY_1S
            LBRA    WAIT_FOR_REBOOT

*******************************************************************************
* OPTIONS_LOOP - Submenu State
*******************************************************************************
OPTIONS_LOOP:
            LBSR    DRAW_OPTIONS
OPT_INPUT:
            JSR     [POLCAT]
            TSTA
            BEQ     OPT_INPUT
            
            CMPA    #'X'
            LBEQ    MAIN_LOOP
            CMPA    #'x'
            LBEQ    MAIN_LOOP
            
            * Sub-menus
            CMPA    #'1'
            LBEQ    LOOP_WIFI
            CMPA    #'2'
            LBEQ    LOOP_ROM
            CMPA    #'3'
            LBEQ    LOOP_DIAG
            CMPA    #'4'
            LBEQ    LOOP_BRIDGE
            CMPA    #'5'
            LBEQ    LOOP_CLEAR
            CMPA    #'6'
            LBEQ    LOOP_TZ_PG1
            CMPA    #'7'
            LBEQ    LOOP_FLASH
            
            BRA     OPT_INPUT

*******************************************************************************
* SUB-MENU LOOPS
*******************************************************************************
LOOP_WIFI:  LBSR    DRAW_WIFI
LW_IN:      JSR     [POLCAT]
            TSTA
            BEQ     LW_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            BRA     LW_IN

LOOP_ROM:   LBSR    DRAW_ROM
LR_IN:      JSR     [POLCAT]
            TSTA
            BEQ     LR_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            BRA     LR_IN

LOOP_INFO:  LBSR    DRAW_INFO
LI_IN:      JSR     [POLCAT]
            TSTA
            BEQ     LI_IN
            CMPA    #'X'
            LBEQ    MAIN_LOOP
            CMPA    #'x'
            LBEQ    MAIN_LOOP
            BRA     LI_IN

LOOP_FLASH: LBSR    DRAW_FLASH
LF_IN:      JSR     [POLCAT]
            TSTA
            BEQ     LF_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            CMPA    #'S'
            LBEQ    LF_SCAN
            CMPA    #'s'
            LBEQ    LF_SCAN
            CMPA    #'C'
            LBEQ    LF_CLEAR
            CMPA    #'c'
            LBEQ    LF_CLEAR
            CMPA    #'R'
            LBEQ    LF_FACTORY
            CMPA    #'r'
            LBEQ    LF_FACTORY
            CMPA    #'I'
            LBEQ    LF_INTERNAL
            CMPA    #'i'
            LBEQ    LF_INTERNAL
            BRA     LF_IN
LF_SCAN:    LDA     #FLASH_CMD_SCAN
            BRA     LF_DO
LF_CLEAR:   LDA     #FLASH_CMD_CLEAR
            BRA     LF_DO
LF_FACTORY: LDA     #FLASH_CMD_FACTORY
            BRA     LF_DO
LF_INTERNAL:LDA     #FLASH_CMD_INTERNAL
LF_DO:      LBSR    FLASH_EXEC
            LBRA    LOOP_FLASH

LOOP_DIAG:  LBSR    DRAW_DIAG
LD_IN:      JSR     [POLCAT]
            TSTA
            BEQ     LD_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            BRA     LD_IN

LOOP_BRIDGE:LBSR    DRAW_BRIDGE
LBR_IN:     JSR     [POLCAT]
            TSTA
            BEQ     LBR_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            BRA     LBR_IN

LOOP_CLEAR: LBSR    DRAW_CLEAR
LC_IN:      JSR     [POLCAT]
            TSTA
            BEQ     LC_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            BRA     LC_IN

LOOP_TZ_PG1: LBSR    DRAW_TZ_PG1
LTZ1_IN:    JSR     [POLCAT]
            TSTA
            BEQ     LTZ1_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            CMPA    #'N'
            LBEQ    LOOP_TZ_PG2
            CMPA    #'n'
            LBEQ    LOOP_TZ_PG2
            CMPA    #'1'
            LBEQ    LTZ1_S1
            CMPA    #'2'
            LBEQ    LTZ1_S2
            CMPA    #'3'
            LBEQ    LTZ1_S3
            CMPA    #'4'
            LBEQ    LTZ1_S4
            CMPA    #'5'
            LBEQ    LTZ1_S5
            CMPA    #'6'
            LBEQ    LTZ1_S6
            BRA     LTZ1_IN
LTZ1_S1:    LDA     #0
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ1_S2:    LDA     #1
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ1_S3:    LDA     #2
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ1_S4:    LDA     #3
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ1_S5:    LDA     #4
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ1_S6:    LDA     #5
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT

LOOP_TZ_PG2: LBSR    DRAW_TZ_PG2
LTZ2_IN:    JSR     [POLCAT]
            TSTA
            BEQ     LTZ2_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            CMPA    #'P'
            LBEQ    LOOP_TZ_PG1
            CMPA    #'p'
            LBEQ    LOOP_TZ_PG1
            CMPA    #'N'
            LBEQ    LOOP_TZ_PG3
            CMPA    #'n'
            LBEQ    LOOP_TZ_PG3
            CMPA    #'1'
            LBEQ    LTZ2_S1
            CMPA    #'2'
            LBEQ    LTZ2_S2
            CMPA    #'3'
            LBEQ    LTZ2_S3
            CMPA    #'4'
            LBEQ    LTZ2_S4
            CMPA    #'5'
            LBEQ    LTZ2_S5
            CMPA    #'6'
            LBEQ    LTZ2_S6
            BRA     LTZ2_IN
LTZ2_S1:    LDA     #6
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ2_S2:    LDA     #7
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ2_S3:    LDA     #8
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ2_S4:    LDA     #9
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ2_S5:    LDA     #10
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ2_S6:    LDA     #11
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT

LOOP_TZ_PG3: LBSR    DRAW_TZ_PG3
LTZ3_IN:    JSR     [POLCAT]
            TSTA
            BEQ     LTZ3_IN
            CMPA    #'X'
            LBEQ    OPTIONS_LOOP
            CMPA    #'x'
            LBEQ    OPTIONS_LOOP
            CMPA    #'P'
            LBEQ    LOOP_TZ_PG2
            CMPA    #'p'
            LBEQ    LOOP_TZ_PG2
            CMPA    #'1'
            LBEQ    LTZ3_S1
            CMPA    #'2'
            LBEQ    LTZ3_S2
            CMPA    #'3'
            LBEQ    LTZ3_S3
            CMPA    #'4'
            LBEQ    LTZ3_S4
            CMPA    #'5'
            LBEQ    LTZ3_S5
            CMPA    #'6'
            LBEQ    LTZ3_S6
            BRA     LTZ3_IN
LTZ3_S1:    LDA     #12
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ3_S2:    LDA     #13
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ3_S3:    LDA     #14
            STA     REG_TZ
            LBRA    TZ_SAVE_EXIT
LTZ3_S4:    LDA     #15
            STA     REG_TZ
            BRA     TZ_SAVE_EXIT
LTZ3_S5:    LDA     #16
            STA     REG_TZ
            BRA     TZ_SAVE_EXIT
LTZ3_S6:    LDA     #17
            STA     REG_TZ
            BRA     TZ_SAVE_EXIT

TZ_SAVE_EXIT:
            LBSR    DRAW_SAVING
            LBSR    WAIT_ANY_KEY
            LDA     #SAVE_MAGIC
            STA     VAR_SAVE
            LBSR    DELAY_1S
            LBRA    WAIT_FOR_REBOOT

*******************************************************************************
* CHECK_SETUP - SDC-DOS missing alert ($D880 bit 0)
*******************************************************************************
CHECK_SETUP:
            LDA     STATUS_FLASH
            BITA    #1
            BEQ     CS_DONE
            LBSR    DRAW_SETUP
CS_WAIT:    JSR     [POLCAT]
            TSTA
            BEQ     CS_WAIT
CS_DONE:    RTS

*******************************************************************************
* FLASH_EXEC - trigger RP2350 flash utility, poll $D881
*******************************************************************************
FLASH_EXEC:
            STA     REG_FLASH_CMD
            LBSR    DRAW_SAVING
FW_WAIT:    LDA     STATUS_RESULT
            CMPA    #RESULT_BUSY
            BEQ     FW_WAIT
            RTS

*******************************************************************************
* WAIT_FOR_SAVE - block until RP2350 flash save finishes
*******************************************************************************
WAIT_FOR_SAVE:
WS_WAIT:    LDA     REG_SAVE_STATUS
            CMPA    #SAVE_STATUS_OK
            BEQ     WS_DONE
            CMPA    #SAVE_STATUS_ERR
            BEQ     WS_DONE
            BRA     WS_WAIT
WS_DONE:    RTS

*******************************************************************************
* DRAW_MAIN
*******************************************************************************
DRAW_MAIN:
            LDX     #$0400
            LDA     #$60
DCLR:       STA     ,X+
            CMPX    #$0600
            BNE     DCLR

            * Row 0: Title (dark)
            LDX     #STR_TITLE
            LDU     #$0400
            LBSR    PSTR_D

            * Row 1: Subtitle (dark)
            LDX     #STR_SUBTITLE
            LDU     #$0420
            LBSR    PSTR_D

            * Row 2: blank separator (black)
            LDU     #$0440
            LDB     #32
            LDA     #$20
DM_BLANK:   STA     ,U+
            DECB
            BNE     DM_BLANK

            * Row 3: Header (dark)
            LDX     #STR_CUR_HDR
            LDU     #$0460
            LBSR    PSTR_D

            * Row 4: Status summary (normal)
            LBSR    DRAW_STATUS

            * Row 5: ROM info (normal)
            LDX     #STR_ROM_LBL
            LDU     #$04A0
            LBSR    PSTR_N
            LDA     VAR_DISK
            BNE     DR4_1
            LDX     #STR_ROM_SDC
            BRA     DR4_P
DR4_1:      LDX     #STR_ROM_NONE
DR4_P:      LBSR    PSTR_N

            * Row 6: WiFi info (normal)
            LDX     #STR_WIFI_LBL
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_WIFI_NA
            LBSR    PSTR_N

            * Row 7: Toggle header (dark)
            LDX     #STR_TOG_HDR
            LDU     #$04E0
            LBSR    PSTR_D

            * Row 8: [A] Audio
            LDX     #STR_ROW_A
            LDU     #$0500
            LBSR    PSTR_T
            LDA     VAR_AUDIO
            CMPA    #0
            BNE     DR8_1
            LDX     #STR_VAL_SPEECH
            BRA     DR8_P
DR8_1:      CMPA    #1
            BNE     DR8_2
            LDX     #STR_VAL_ORCH90
            BRA     DR8_P
DR8_2:      LDX     #STR_VAL_OFF
DR8_P:      LBSR    PSTR_N

            * Row 9: [V] Video
            LDX     #STR_ROW_V
            LDU     #$0520
            LBSR    PSTR_T
            LDA     VAR_VIDEO
            CMPA    #0
            BNE     DR9_1
            LDX     #STR_VAL_V9958
            BRA     DR9_P
DR9_1:      CMPA    #1
            BNE     DR9_2
            LDX     #STR_VAL_SUPERSPR
            BRA     DR9_P
DR9_2:      LDX     #STR_VAL_OFF
DR9_P:      LBSR    PSTR_N

            * Row 10: [C] Comm
            LDX     #STR_ROW_C
            LDU     #$0540
            LBSR    PSTR_T
            LDA     VAR_COMM
            CMPA    #0
            BNE     DRA_1
            LDX     #STR_VAL_WIFI
            BRA     DRA_P
DRA_1:      CMPA    #1
            BNE     DRA_2
            LDX     #STR_VAL_RS232
            BRA     DRA_P
DRA_2:      CMPA    #2
            BNE     DRA_3
            LDX     #STR_VAL_FUJINET
            BRA     DRA_P
DRA_3:      LDX     #STR_VAL_OFF
DRA_P:      LBSR    PSTR_N

            * Row 11: [D] Disk
            LDX     #STR_ROW_D
            LDU     #$0560
            LBSR    PSTR_T
            LDA     VAR_DISK
            CMPA    #0
            BNE     DRB_1
            LDX     #STR_VAL_SDC
            BRA     DRB_P
DRB_1:      CMPA    #1
            BNE     DRB_2
            LDX     #STR_VAL_FUJINET_D
            BRA     DRB_P
DRB_2:      CMPA    #2
            BNE     DRB_3
            LDX     #STR_VAL_OFF
            BRA     DRB_P
DRB_3:      LDX     #STR_VAL_INTERNAL
DRB_P:      LBSR    PSTR_N

            * Row 12: [R] RTC
            LDX     #STR_ROW_R
            LDU     #$0580
            LBSR    PSTR_T
            LDA     VAR_RTC
            BNE     DRC_1
            LDX     #STR_VAL_RTCOFF
            BRA     DRC_P
DRC_1:      LDX     #STR_VAL_RTCON
DRC_P:      LBSR    PSTR_N

            * Row 13: Divider (dark)
            LDX     #STR_DIV_FULL
            LDU     #$05A0
            LBSR    PSTR_D

            * Row 14: ENTER footer (toggle)
            LDX     #STR_FOOT1
            LDU     #$05C0
            LBSR    PSTR_T

            * Row 15: Options/Info footer (toggle)
            LDX     #STR_FOOT2
            LDU     #$05E0
            LBSR    PSTR_T

            RTS

*******************************************************************************
* DRAW_OPTIONS
*******************************************************************************
DRAW_OPTIONS:
            * Clear screen: fill $0400-$05FF with $60 (solid green)
            LDX     #$0400
            LDA     #$60
DOCLR:      STA     ,X+
            CMPX    #$0600
            BNE     DOCLR

            * Row 0: Title (dark)
            LDX     #STR_OPT_TITLE
            LDU     #$0400
            LBSR    PSTR_D

            * Row 2: Header (dark)
            LDX     #STR_OPT_HDR
            LDU     #$0440
            LBSR    PSTR_D

            * Rows 3-9: Options (mixed)
            LDX     #STR_OPT_1
            LDU     #$0460
            LBSR    PSTR_T
            
            LDX     #STR_OPT_2
            LDU     #$0480
            LBSR    PSTR_T
            
            LDX     #STR_OPT_3
            LDU     #$04A0
            LBSR    PSTR_T
            
            LDX     #STR_OPT_4
            LDU     #$04C0
            LBSR    PSTR_T
            
            LDX     #STR_OPT_5
            LDU     #$04E0
            LBSR    PSTR_T
            
            LDX     #STR_OPT_6
            LDU     #$0500
            LBSR    PSTR_T

            LDX     #STR_OPT_7
            LDU     #$0520
            LBSR    PSTR_T

            * Row 10: Divider (dark)
            LDX     #STR_DIV_OPT
            LDU     #$0540
            LBSR    PSTR_D

            * Row 11: Return footer (mixed)
            LDX     #STR_OPT_FOOT
            LDU     #$0560
            LBSR    PSTR_T

            * Row 12: ESP32 FW (normal)
            LDX     #STR_OPT_FW
            LDU     #$0580
            LBSR    PSTR_N
            * Append firmware buffer
            LDX     #STATUS_FW
            LDA     ,X
            BNE     DO_FW
            LDX     #STR_UNK
DO_FW:      LBSR    PSTR_N_MAX

            * Row 13: SD Card (normal)
            LDX     #STR_OPT_SD
            LDU     #$05A0
            LBSR    PSTR_N
            * Append SD buffer
            LDX     #STATUS_SD
            LDA     ,X
            BNE     DO_SD
            LDX     #STR_UNK
DO_SD:      LBSR    PSTR_N_MAX

            RTS

*******************************************************************************
* DRAW_WIFI
*******************************************************************************
DRAW_WIFI:
            LDX     #$0400
            LDA     #$60
DW_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DW_CLR

            LDX     #STR_WIFI_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_WIFI_H
            LDU     #$0440
            LBSR    PSTR_D
            LDX     #STR_WIFI_1
            LDU     #$0480
            LBSR    PSTR_T
            LDX     #STR_WIFI_2
            LDU     #$04A0
            LBSR    PSTR_T
            LDX     #STR_WIFI_3
            LDU     #$04C0
            LBSR    PSTR_T
            LDX     #STR_WIFI_4
            LDU     #$04E0
            LBSR    PSTR_T
            LDX     #STR_WIFI_5
            LDU     #$0500
            LBSR    PSTR_T
            LDX     #STR_DIV_OPT
            LDU     #$0540
            LBSR    PSTR_D
            LDX     #STR_WIFI_S
            LDU     #$0560
            LBSR    PSTR_T
            LDX     #STR_WIFI_J
            LDU     #$0580
            LBSR    PSTR_T
            LDX     #STR_DIV_OPT
            LDU     #$05A0
            LBSR    PSTR_D
            LDX     #STR_RTN_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_SETUP - missing SDC-DOS alert
*******************************************************************************
DRAW_SETUP:
            LDX     #$0400
            LDA     #$60
DSU_CLR:    STA     ,X+
            CMPX    #$0600
            BNE     DSU_CLR

            LDX     #STR_SETUP_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_SETUP_H
            LDU     #$0440
            LBSR    PSTR_D
            LDX     #STR_SETUP_1
            LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_SETUP_2
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_SETUP_3
            LDU     #$04E0
            LBSR    PSTR_N
            LDX     #STR_SETUP_4
            LDU     #$0500
            LBSR    PSTR_N
            LDX     #STR_SETUP_5
            LDU     #$0520
            LBSR    PSTR_N
            LDX     #STR_DIV_FULL
            LDU     #$05C0
            LBSR    PSTR_D
            LDX     #STR_SETUP_6
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_FLASH - Flash ROM bank management
*******************************************************************************
DRAW_FLASH:
            LDX     #$0400
            LDA     #$60
DF_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DF_CLR

            LDX     #STR_FLASH_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_FLASH_H
            LDU     #$0440
            LBSR    PSTR_D
            LDX     #STR_FLASH_S0
            LDU     #$0460
            LBSR    PSTR_N
            LDA     STATUS_FLASH
            BITA    #1
            BEQ     DF_S1OK
            LDX     #STR_FLASH_S1M
            BRA     DF_S1P
DF_S1OK:    LDX     #STR_FLASH_S1I
DF_S1P:     LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_FLASH_S2
            LDU     #$04A0
            LBSR    PSTR_N
            LDX     #STR_FLASH_S3
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_FLASH_S
            LDU     #$0500
            LBSR    PSTR_T
            LDX     #STR_FLASH_C
            LDU     #$0520
            LBSR    PSTR_T
            LDX     #STR_FLASH_R
            LDU     #$0540
            LBSR    PSTR_T
            LDX     #STR_FLASH_I
            LDU     #$0560
            LBSR    PSTR_T
            LDX     #STR_DIV_FULL
            LDU     #$05A0
            LBSR    PSTR_D
            LDX     #STR_RTN_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_INFO - Credits and hardware info
*******************************************************************************
DRAW_INFO:
            LDX     #$0400
            LDA     #$60
DI_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DI_CLR

            LDX     #STR_INFO_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_INFO_HDR
            LDU     #$0420
            LBSR    PSTR_D

            * Row 2: blank separator (black)
            LDU     #$0440
            LDB     #32
            LDA     #$20
DI_BLANK:   STA     ,U+
            DECB
            BNE     DI_BLANK

            LDX     #STR_INFO_BOARD
            LDU     #$0460
            LBSR    PSTR_N
            LDX     #STR_INFO_2
            LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_INFO_3
            LDU     #$04A0
            LBSR    PSTR_N
            LDX     #STR_INFO_4
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_DIV_FULL
            LDU     #$04E0
            LBSR    PSTR_D
            LDX     #STR_INFO_5
            LDU     #$0500
            LBSR    PSTR_N
            LDX     #STR_INFO_6
            LDU     #$0520
            LBSR    PSTR_N
            LDX     #STR_DIV_FULL
            LDU     #$0540
            LBSR    PSTR_D
            LDX     #STR_INFO_MARK1
            LDU     #$0560
            LBSR    PSTR_N
            LDX     #STR_INFO_MARK2
            LDU     #$0580
            LBSR    PSTR_N
            LDX     #STR_INFO_URL
            LDU     #$05A0
            LBSR    PSTR_N
            LDX     #STR_DIV_FULL
            LDU     #$05C0
            LBSR    PSTR_D
            LDX     #STR_RTN_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_ROM - Alt ROM loader placeholder
*******************************************************************************
DRAW_ROM:
            LDX     #$0400
            LDA     #$60
DR_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DR_CLR

            LDX     #STR_ROM_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_ROM_MSG
            LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_RTN_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_DIAG
*******************************************************************************
DRAW_DIAG:
            LDX     #$0400
            LDA     #$60
DD_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DD_CLR

            LDX     #STR_DIAG_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_DIAG_H
            LDU     #$0440
            LBSR    PSTR_D
            LDX     #STR_DIAG_1
            LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_DIAG_2
            LDU     #$04A0
            LBSR    PSTR_N
            LDX     #STR_DIAG_3
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_DIAG_4
            LDU     #$04E0
            LBSR    PSTR_N
            LDX     #STR_DIAG_5
            LDU     #$0500
            LBSR    PSTR_N
            LDX     #STR_DIV_OPT
            LDU     #$0540
            LBSR    PSTR_D
            LDX     #STR_DIAG_R
            LDU     #$0560
            LBSR    PSTR_T
            LDX     #STR_DIV_OPT
            LDU     #$05A0
            LBSR    PSTR_D
            LDX     #STR_RTN_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_BRIDGE
*******************************************************************************
DRAW_BRIDGE:
            LDX     #$0400
            LDA     #$60
DB_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DB_CLR

            LDX     #STR_BRG_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_BRG_H
            LDU     #$0440
            LBSR    PSTR_D
            LDX     #STR_BRG_1
            LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_BRG_2
            LDU     #$04A0
            LBSR    PSTR_N
            LDX     #STR_BRG_3
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_BRG_4
            LDU     #$04E0
            LBSR    PSTR_N
            LDX     #STR_BRG_5
            LDU     #$0500
            LBSR    PSTR_N
            LDX     #STR_DIV_OPT
            LDU     #$0540
            LBSR    PSTR_D
            LDX     #STR_BRG_P
            LDU     #$0560
            LBSR    PSTR_T
            LDX     #STR_DIV_OPT
            LDU     #$05A0
            LBSR    PSTR_D
            LDX     #STR_RTN_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_CLEAR
*******************************************************************************
DRAW_CLEAR:
            LDX     #$0400
            LDA     #$60
DC_CLR:     STA     ,X+
            CMPX    #$0600
            BNE     DC_CLR

            LDX     #STR_CLR_T
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_CLR_H
            LDU     #$0440
            LBSR    PSTR_D
            LDX     #STR_CLR_1
            LDU     #$0480
            LBSR    PSTR_N
            LDX     #STR_CLR_2
            LDU     #$04A0
            LBSR    PSTR_N
            LDX     #STR_CLR_3
            LDU     #$04C0
            LBSR    PSTR_N
            LDX     #STR_CLR_4
            LDU     #$0500
            LBSR    PSTR_N
            LDX     #STR_CLR_5
            LDU     #$0520
            LBSR    PSTR_N
            LDX     #STR_DIV_OPT
            LDU     #$0560
            LBSR    PSTR_D
            LDX     #STR_CLR_Y
            LDU     #$0580
            LBSR    PSTR_T
            LDX     #STR_DIV_OPT
            LDU     #$05A0
            LBSR    PSTR_D
            LDX     #STR_CLR_X
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_TZ_PG1
*******************************************************************************
DRAW_TZ_PG1:
            LDX     #$0400
            LDA     #$60
DTZ1_CLR:   STA     ,X+
            CMPX    #$0600
            BNE     DTZ1_CLR

            LDX     #STR_TZ_T1
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_TZ_H
            LDU     #$0440
            LBSR    PSTR_D
            
            LDX     #STR_TZ_SEL
            LDU     #$0480
            LBSR    PSTR_N

            LDX     #STR_TZ_1
            LDU     #$04A0
            LBSR    PSTR_T
            LDX     #STR_TZ_2
            LDU     #$04C0
            LBSR    PSTR_T
            LDX     #STR_TZ_3
            LDU     #$04E0
            LBSR    PSTR_T
            LDX     #STR_TZ_4
            LDU     #$0500
            LBSR    PSTR_T
            LDX     #STR_TZ_5
            LDU     #$0520
            LBSR    PSTR_T
            LDX     #STR_TZ_6
            LDU     #$0540
            LBSR    PSTR_T
            LDU     #$0560
            LBSR    CLR_ROW_G
            LDX     #STR_DIV_OPT
            LDU     #$0580
            LBSR    PSTR_D
            LDU     #$05A0
            LBSR    CLR_ROW_G
            LDX     #STR_TZ_PROMPT
            LDU     #$05C0
            LBSR    PSTR_N
            LDX     #STR_RTN_N
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_TZ_PG2
*******************************************************************************
DRAW_TZ_PG2:
            LDX     #$0400
            LDA     #$60
DTZ2_CLR:   STA     ,X+
            CMPX    #$0600
            BNE     DTZ2_CLR

            LDX     #STR_TZ_T2
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_TZ_H
            LDU     #$0440
            LBSR    PSTR_D
            
            LDX     #STR_TZ_SEL
            LDU     #$0480
            LBSR    PSTR_N

            LDX     #STR_TZ_7
            LDU     #$04A0
            LBSR    PSTR_T
            LDX     #STR_TZ_8
            LDU     #$04C0
            LBSR    PSTR_T
            LDX     #STR_TZ_9
            LDU     #$04E0
            LBSR    PSTR_T
            LDX     #STR_TZ_10
            LDU     #$0500
            LBSR    PSTR_T
            LDX     #STR_TZ_11
            LDU     #$0520
            LBSR    PSTR_T
            LDX     #STR_TZ_12
            LDU     #$0540
            LBSR    PSTR_T
            LDU     #$0560
            LBSR    CLR_ROW_G
            LDX     #STR_DIV_OPT
            LDU     #$0580
            LBSR    PSTR_D
            LDU     #$05A0
            LBSR    CLR_ROW_G
            LDX     #STR_TZ_PROMPT
            LDU     #$05C0
            LBSR    PSTR_N
            LDX     #STR_RTN_NP
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* DRAW_TZ_PG3
*******************************************************************************
DRAW_TZ_PG3:
            LDX     #$0400
            LDA     #$60
DTZ3_CLR:   STA     ,X+
            CMPX    #$0600
            BNE     DTZ3_CLR

            LDX     #STR_TZ_T3
            LDU     #$0400
            LBSR    PSTR_D
            LDX     #STR_TZ_H
            LDU     #$0440
            LBSR    PSTR_D
            
            LDX     #STR_TZ_SEL
            LDU     #$0480
            LBSR    PSTR_N

            LDX     #STR_TZ_13
            LDU     #$04A0
            LBSR    PSTR_T
            LDX     #STR_TZ_14
            LDU     #$04C0
            LBSR    PSTR_T
            LDX     #STR_TZ_15
            LDU     #$04E0
            LBSR    PSTR_T
            LDX     #STR_TZ_16
            LDU     #$0500
            LBSR    PSTR_T
            LDX     #STR_TZ_17
            LDU     #$0520
            LBSR    PSTR_T
            LDX     #STR_TZ_18
            LDU     #$0540
            LBSR    PSTR_T
            LDU     #$0560
            LBSR    CLR_ROW_G
            LDX     #STR_DIV_OPT
            LDU     #$0580
            LBSR    PSTR_D
            LDU     #$05A0
            LBSR    CLR_ROW_G
            LDX     #STR_TZ_PROMPT
            LDU     #$05C0
            LBSR    PSTR_N
            LDX     #STR_RTN_P
            LDU     #$05E0
            LBSR    PSTR_T
            RTS

*******************************************************************************
* Value-string helpers for save summary (X -> string, A clobbered)
*******************************************************************************
PVAL_AUDIO:
            LDA     VAR_AUDIO
            CMPA    #0
            BNE     PVA_1
            LDX     #STR_VAL_SPEECH
            RTS
PVA_1:      CMPA    #1
            BNE     PVA_2
            LDX     #STR_VAL_ORCH90
            RTS
PVA_2:      LDX     #STR_VAL_OFF
            RTS

PVAL_VIDEO:
            LDA     VAR_VIDEO
            CMPA    #0
            BNE     PVV_1
            LDX     #STR_VAL_V9958
            RTS
PVV_1:      CMPA    #1
            BNE     PVV_2
            LDX     #STR_VAL_SUPERSPR
            RTS
PVV_2:      LDX     #STR_VAL_OFF
            RTS

PVAL_COMM:
            LDA     VAR_COMM
            CMPA    #0
            BNE     PVC_1
            LDX     #STR_VAL_WIFI
            RTS
PVC_1:      CMPA    #1
            BNE     PVC_2
            LDX     #STR_VAL_RS232
            RTS
PVC_2:      CMPA    #2
            BNE     PVC_3
            LDX     #STR_VAL_FUJINET
            RTS
PVC_3:      LDX     #STR_VAL_OFF
            RTS

PVAL_DISK:
            LDA     VAR_DISK
            CMPA    #0
            BNE     PVD_1
            LDX     #STR_VAL_SDC
            RTS
PVD_1:      CMPA    #1
            BNE     PVD_2
            LDX     #STR_VAL_FUJINET_D
            RTS
PVD_2:      CMPA    #2
            BNE     PVD_3
            LDX     #STR_VAL_OFF
            RTS
PVD_3:      LDX     #STR_VAL_INTERNAL
            RTS

PVAL_RTC:
            LDA     VAR_RTC
            BNE     PVR_1
            LDX     #STR_VAL_RTCOFF
            RTS
PVR_1:      LDX     #STR_VAL_RTCON
            RTS

PVAL_TZ:
            LDA     REG_TZ
            ASLA
            LDX     #TZ_NAME_TAB
            LDX     A,X
            RTS

*******************************************************************************
* DRAW_SAVING
*******************************************************************************
DRAW_SAVING:
            LDX     #$0400
            LDA     #$60
DSAV_CLR:   STA     ,X+
            CMPX    #$0600
            BNE     DSAV_CLR

            LDX     #STR_SAVING
            LDU     #$0400
            LBSR    PSTR_D

            LDX     #STR_SAVE_SUB
            LDU     #$0420
            LBSR    PSTR_D

            LDU     #$0440
            LDX     #STR_SAVE_LA
            LBSR    PSTR_N
            LBSR    PVAL_AUDIO
            LBSR    PSTR_N

            LDU     #$0460
            LDX     #STR_SAVE_LV
            LBSR    PSTR_N
            LBSR    PVAL_VIDEO
            LBSR    PSTR_N

            LDU     #$0480
            LDX     #STR_SAVE_LC
            LBSR    PSTR_N
            LBSR    PVAL_COMM
            LBSR    PSTR_N

            LDU     #$04A0
            LDX     #STR_SAVE_LD
            LBSR    PSTR_N
            LBSR    PVAL_DISK
            LBSR    PSTR_N

            LDU     #$04C0
            LDX     #STR_SAVE_LR
            LBSR    PSTR_N
            LBSR    PVAL_RTC
            LBSR    PSTR_N

            LDU     #$04E0
            LDX     #STR_SAVE_LT
            LBSR    PSTR_N
            LBSR    PVAL_TZ
            LBSR    PSTR_N

            LDU     #$05C0
            LDX     #STR_SAVE_CONT
            LBSR    PSTR_T

            RTS

*******************************************************************************
* WAIT_ANY_KEY
*******************************************************************************
WAIT_ANY_KEY:
WAK_LOOP:   JSR     [POLCAT]
            TSTA
            BEQ     WAK_LOOP
            RTS

*******************************************************************************
* WAIT_FOR_REBOOT - RP2350 pulses RESET after save commit
*******************************************************************************
WAIT_FOR_REBOOT:
            BRA     WAIT_FOR_REBOOT

*******************************************************************************
* DELAY_1S
*******************************************************************************
DELAY_1S:
            LDB     #2
D1S_OUTER:  LDX     #$FFFF
D1S_LOOP:   LEAX    -1,X
            BNE     D1S_LOOP
            DECB
            BNE     D1S_OUTER
            RTS

* DRAW_STATUS - Row 4 dynamic (normal text)
*******************************************************************************
DRAW_STATUS:
            LDU     #$0480
            LDX     #STR_STAT_PRE
            LBSR    PSTR_N
            LDA     VAR_AUDIO
            CMPA    #0
            BNE     DS_A1
            LDX     #STR_STAT_SPK
            BRA     DS_AP
DS_A1:      CMPA    #1
            BNE     DS_A2
            LDX     #STR_STAT_ORCH
            BRA     DS_AP
DS_A2:      LDX     #STR_STAT_OFF
DS_AP:      LBSR    PSTR_N

            LDX     #STR_STAT_D
            LBSR    PSTR_N
            LDA     VAR_DISK
            CMPA    #0
            BNE     DS_D1
            LDX     #STR_STAT_SDC
            BRA     DS_DP
DS_D1:      CMPA    #1
            BNE     DS_D2
            LDX     #STR_STAT_FUJ
            BRA     DS_DP
DS_D2:      CMPA    #2
            BNE     DS_D3
            LDX     #STR_STAT_OFF
            BRA     DS_DP
DS_D3:      LDX     #STR_STAT_INT
DS_DP:      LBSR    PSTR_N

            LDX     #STR_STAT_V
            LBSR    PSTR_N
            LDA     VAR_VIDEO
            CMPA    #0
            BNE     DS_V1
            LDX     #STR_STAT_V99
            BRA     DS_VP
DS_V1:      CMPA    #1
            BNE     DS_V2
            LDX     #STR_STAT_SPR
            BRA     DS_VP
DS_V2:      LDX     #STR_STAT_REG
DS_VP:      LBSR    PSTR_N

            LDX     #STR_STAT_C
            LBSR    PSTR_N
            LDA     VAR_COMM
            CMPA    #0
            BNE     DS_C1
            LDX     #STR_STAT_WI
            BRA     DS_CP
DS_C1:      CMPA    #1
            BNE     DS_C2
            LDX     #STR_STAT_RS
            BRA     DS_CP
DS_C2:      CMPA    #2
            BNE     DS_C3
            LDX     #STR_STAT_FUJC
            BRA     DS_CP
DS_C3:      LDX     #STR_STAT_OFF
DS_CP:      LBSR    PSTR_N
            RTS

*******************************************************************************
* CLR_ROW - fill 32 columns at U with black spaces
*******************************************************************************
CLR_ROW:    LDB     #32
            LDA     #$20
CR_LOOP:    STA     ,U+
            DECB
            BNE     CR_LOOP
            RTS

*******************************************************************************
* CLR_ROW_G - fill 32 columns at U with green spaces ($60)
*******************************************************************************
CLR_ROW_G:  LDB     #32
            LDA     #$60
CRG_LOOP:   STA     ,U+
            DECB
            BNE     CRG_LOOP
            RTS

*******************************************************************************
* PSTR_D - Inverse: green on black (bit 6 = 0)
*******************************************************************************
PSTR_D:     LDA     ,X+
            BEQ     PD_DONE
            ANDA    #$3F
            STA     ,U+
            BRA     PSTR_D
PD_DONE:    RTS

*******************************************************************************
* PSTR_N - Normal: black on green (bit 6 = 1)
*******************************************************************************
PSTR_N:     LDA     ,X+
            BEQ     PN_DONE
            ANDA    #$3F
            ORA     #$40
            STA     ,U+
            BRA     PSTR_N
PN_DONE:    RTS

*******************************************************************************
* PSTR_T - Toggle mode. Starts normal. $01 toggles normal<->inverse.
*   Use for rows with [A] hotkey highlights embedded in normal text.
*******************************************************************************
PSTR_T:     LDA     #$40
            STA     VAR_TMODE       * Start in normal mode
PT_LOOP:    LDA     ,X+
            BEQ     PT_DONE
            CMPA    #$01            * Toggle marker?
            BEQ     PT_TOG
            ANDA    #$3F            * 6-bit char shape
            ORA     VAR_TMODE       * Apply current mode
            STA     ,U+
            BRA     PT_LOOP
PT_TOG:     LDA     VAR_TMODE
            EORA    #$40            * Flip bit 6
            STA     VAR_TMODE
            BRA     PT_LOOP
PT_DONE:    RTS

*******************************************************************************
* PSTR_N_MAX - Normal, max 16 chars (for ESP32 FW and SD status)
*******************************************************************************
PSTR_N_MAX: LDB     #16
PNNM_L:     LDA     ,X+
            BEQ     PNNM_DONE
            ANDA    #$3F
            ORA     #$40
            STA     ,U+
            DECB
            BNE     PNNM_L
PNNM_DONE:  RTS

*******************************************************************************
* STRINGS - $01 = toggle marker for PSTR_T
*******************************************************************************

STR_TITLE   FCC     "    COPICO 10 IN 1 MULTI HAT     "
            FCB     0

STR_SUBTITLE FCC    "      CREATED BY PORTACOCO      "
            FCB     0

STR_CUR_HDR FCC     "--------CURRENT SETTINGS--------"
            FCB     0

STR_TOG_HDR FCC     "------PRESS KEY TO TOGGLE-------"
            FCB     0

STR_DIV_FULL FCC    "--------------------------------"
            FCB     0

STR_ROM_LBL FCC     "     ROM: [ "
            FCB     0
STR_ROM_SDC FCC     "COCOSDC V1.1 ]     "
            FCB     0
STR_ROM_NONE FCC    "NONE         ]     "
            FCB     0

STR_WIFI_LBL FCC    "    WIFI: "
            FCB     0
STR_WIFI_NA FCC     "PRESS "
            FCB     $22
            FCC     "O"
            FCB     $22
            FCC     " TO SETUP    "
            FCB     0

* Status row fragments (normal)
STR_STAT_PRE FCC    "   A:"
            FCB     0
STR_STAT_SPK FCC    "SSC"
            FCB     0
STR_STAT_ORCH FCC   "ORC"
            FCB     0
STR_STAT_D  FCC     " D:"
            FCB     0
STR_STAT_SDC FCC    "SDC"
            FCB     0
STR_STAT_FUJ FCC    "FUJ"
            FCB     0
STR_STAT_V  FCC     " V:"
            FCB     0
STR_STAT_REG FCC    "REG"
            FCB     0
STR_STAT_V99 FCC    "WP2"
            FCB     0
STR_STAT_SPR FCC    "SPR"
            FCB     0
STR_STAT_C  FCC     " C:"
            FCB     0
STR_STAT_WI FCC     "WIM"
            FCB     0
STR_STAT_RS FCC     "232"
            FCB     0
STR_STAT_FUJC FCC   "FUJ"
            FCB     0
STR_STAT_OFF FCC    "OFF"
            FCB     0
STR_STAT_INT FCC    "INT"
            FCB     0

* Toggle row labels: $01 wraps the inverse hotkey letter
STR_ROW_A   FCC     "   "
            FCB     $01
            FCC     "[A]"
            FCB     $01
            FCC     " AUDIO: "
            FCB     0
STR_ROW_V   FCC     "   "
            FCB     $01
            FCC     "[V]"
            FCB     $01
            FCC     " VIDEO: "
            FCB     0
STR_ROW_C   FCC     "   "
            FCB     $01
            FCC     "[C]"
            FCB     $01
            FCC     " COMM : "
            FCB     0
STR_ROW_D   FCC     "   "
            FCB     $01
            FCC     "[D]"
            FCB     $01
            FCC     " DISK : "
            FCB     0
STR_ROW_R   FCC     "   "
            FCB     $01
            FCC     "[R]"
            FCB     $01
            FCC     " RTC  : "
            FCB     0

* Toggle values (rendered with PSTR_N)
STR_VAL_SPEECH  FCC "SPEECH/SOUND CARD "
                FCB 0
STR_VAL_ORCH90  FCC "ORCHESTRA 90      "
                FCB 0
STR_VAL_OFF     FCC "REGULAR COCO      "
                FCB 0
STR_VAL_REGCOCO FCC "REGULAR COCO      "
                FCB 0
STR_VAL_V9958   FCC "WORDPAK 2+        "
                FCB 0
STR_VAL_SUPERSPR FCC "SUPER SPRITE FM   "
                 FCB 0
STR_VAL_WIFI    FCC "WIMODEM(RS232 PAK)"
                FCB 0
STR_VAL_RS232   FCC "RS-232 PAC        "
                FCB 0
STR_VAL_FUJINET FCC "FUJINET           "
                FCB 0
STR_VAL_SDC     FCC "COCOSDC (SD)      "
                FCB 0
STR_VAL_FUJINET_D FCC "FUJINET           "
                  FCB 0
STR_VAL_INTERNAL FCC "INTERNAL ROM PASS "
                   FCB 0
STR_VAL_RTCOFF  FCC "OFF (NO WIFI)     "
                FCB 0
STR_VAL_RTCON   FCC "ON  (SYNCED)      "
                FCB 0

* Footer rows with toggle markers (UPPERCASE inside toggles — CoCo 6-bit)
STR_FOOT1   FCC     " PRESS "
            FCB     $01
            FCC     "[ENTER]"
            FCB     $01
            FCC     " TO RESTART COCO "
            FCB     0
STR_FOOT2   FCC     "     "
            FCB     $01
            FCC     "[O]"
            FCB     $01
            FCC     " OPTIONS OR "
            FCB     $01
            FCC     "[I]"
            FCB     $01
            FCC     " INFO"
            FCB     0

* --- Options Menu Strings ---
STR_OPT_TITLE FCC   "    *** ADVANCED OPTIONS ***    "
              FCB   0

STR_OPT_HDR FCC     "  -------SYSTEM UTILITIES-------"
            FCB     0

STR_OPT_1   FCC     "   "
            FCB     $01
            FCC     "[1]"
            FCB     $01
            FCC     " WIFI CONFIG (SCAN/JOIN)  "
            FCB     0

STR_OPT_2   FCC     "   "
            FCB     $01
            FCC     "[2]"
            FCB     $01
            FCC     " ALT ROM LOADER (/ROMS)   "
            FCB     0

STR_OPT_3   FCC     "   "
            FCB     $01
            FCC     "[3]"
            FCB     $01
            FCC     " BUS SNIFFER DIAGNOSTICS  "
            FCB     0

STR_OPT_4   FCC     "   "
            FCB     $01
            FCC     "[4]"
            FCB     $01
            FCC     " ESP32 BRIDGE STATUS      "
            FCB     0

STR_OPT_5   FCC     "   "
            FCB     $01
            FCC     "[5]"
            FCB     $01
            FCC     " CLEAR WIFI SETTINGS      "
            FCB     0

STR_OPT_6   FCC     "   "
            FCB     $01
            FCC     "[6]"
            FCB     $01
            FCC     " TIME ZONE CONFIGURATION  "
            FCB     0

STR_OPT_7   FCC     "   "
            FCB     $01
            FCC     "[7]"
            FCB     $01
            FCC     " FLASH ROM MANAGEMENT     "
            FCB     0

STR_DIV_OPT FCC     "------------------------------- "
            FCB     0

STR_OPT_FOOT FCC    "    PRESS "
             FCB    $01
             FCC    "[X]"
             FCB    $01
             FCC    " FOR MAIN MENU     "
             FCB    0

STR_OPT_FW  FCC     "    ESP32 FW: "
            FCB     0

STR_OPT_SD  FCC     "    SD CARD : "
            FCB     0

STR_UNK     FCC     "UNKNOWN         "
            FCB     0

* --- WIFI Config Strings ---
STR_WIFI_T  FCC     "   *** WIFI CONFIGURATION ***   "
            FCB     0
STR_WIFI_H  FCC     " ----AVAILABLE NETWORKS (SCAN)--"
            FCB     0
STR_WIFI_1  FCC     "   "
            FCB     $01
            FCC     "[1]"
            FCB     $01
            FCC     " MY_HOME_WIFI   (STR: 85%)"
            FCB     0
STR_WIFI_2  FCC     "   "
            FCB     $01
            FCC     "[2]"
            FCB     $01
            FCC     " GUEST_NET      (STR: 60%)"
            FCB     0
STR_WIFI_3  FCC     "   "
            FCB     $01
            FCC     "[3]"
            FCB     $01
            FCC     " XFINITYWIFI    (STR: 40%)"
            FCB     0
STR_WIFI_4  FCC     "   "
            FCB     $01
            FCC     "[4]"
            FCB     $01
            FCC     " -- EMPTY --              "
            FCB     0
STR_WIFI_5  FCC     "   "
            FCB     $01
            FCC     "[5]"
            FCB     $01
            FCC     " -- EMPTY --              "
            FCB     0
STR_WIFI_S  FCC     "   "
            FCB     $01
            FCC     "[S]"
            FCB     $01
            FCC     " SCAN AGAIN               "
            FCB     0
STR_WIFI_J  FCC     "   "
            FCB     $01
            FCC     "[J]"
            FCB     $01
            FCC     " JOIN SELECTED (PROMPT PW)"
            FCB     0

* --- Setup Required alert ---
STR_SETUP_T FCC     "  *** SETUP REQUIRED ***        "
            FCB     0
STR_SETUP_H FCC     "-------- ACTION NEEDED --------"
            FCB     0
STR_SETUP_1 FCC     "  SDC-DOS ROM IS NOT INSTALLED. "
            FCB     0
STR_SETUP_2 FCC     "  PLEASE PLACE COCOSDC.ROM IN   "
            FCB     0
STR_SETUP_3 FCC     "  ROOT OR /ROMS/ FOLDER ON SD  "
            FCB     0
STR_SETUP_4 FCC     "  CARD AND RESTART TO INSTALL   "
            FCB     0
STR_SETUP_5 FCC     "  SDC-DOS.                      "
            FCB     0
STR_SETUP_6 FCC     "   PRESS ANY KEY TO RESTART    "
            FCB     0

* --- Flash ROM Management ---
STR_FLASH_T FCC     "  *** FLASH ROM MANAGEMENT ***  "
            FCB     0
STR_FLASH_H FCC     "  ----  FLASH BANK STATUS  ---- "
            FCB     0
STR_FLASH_S0 FCC    "  [SLOT 0] X-BIOS     : INSTALLED"
            FCB     0
STR_FLASH_S1I FCC   "  [SLOT 1] SDC-DOS    : INSTALLED"
            FCB     0
STR_FLASH_S1M FCC   "  [SLOT 1] SDC-DOS    : MISSING  "
            FCB     0
STR_FLASH_S2 FCC    "  [SLOT 2] FUJINET    : INSTALLED"
            FCB     0
STR_FLASH_S3 FCC    "  [SLOT 3] RS-232 PAK : INSTALLED"
            FCB     0
STR_FLASH_S FCC     "   "
            FCB     $01
            FCC     "[S]"
            FCB     $01
            FCC     " SCAN SD FOR ROMS         "
            FCB     0
STR_FLASH_C FCC     "   "
            FCB     $01
            FCC     "[C]"
            FCB     $01
            FCC     " CLEAR FLASH BANK         "
            FCB     0
STR_FLASH_R FCC     "   "
            FCB     $01
            FCC     "[R]"
            FCB     $01
            FCC     " RE-INSTALL DEFAULTS      "
            FCB     0
STR_FLASH_I FCC     "   "
            FCB     $01
            FCC     "[I]"
            FCB     $01
            FCC     " BOOT INTERNAL ROM (PASS) "
            FCB     0

* --- Info page (all uppercase — CoCo 6-bit charset, 32 cols centered) ---
STR_INFO_T  FCC     "*** COPICO 10 IN 1 MULTI HAT ***"
            FCB     0
STR_INFO_HDR FCC    "   SOFTWARE & HARDWARE CREDITS  "
            FCB     0
STR_INFO_BOARD FCC  "   COPICO CENTIPEDE BOARD BY    "
            FCB     0
STR_INFO_2  FCC     "     HENRY STRICKLAND AND       "
            FCB     0
STR_INFO_3  FCC     "         THOMAS SHANKS          "
            FCB     0
STR_INFO_4  FCC     "  GITHUB.COM/STRICKYAK/COPICO   "
            FCB     0
STR_INFO_5  FCC     "  THIS HAT IS DESIGNED FOR THE  "
            FCB     0
STR_INFO_6  FCC     "    CENTIPEDE 32Z BOARD ONLY    "
            FCB     0
STR_INFO_MARK1 FCC  "   10 IN 1 HARDWARE AND ROM     "
            FCB     0
STR_INFO_MARK2 FCC  "     SOFTWARE BY MARK PACAN     "
            FCB     0
STR_INFO_URL FCC    "         WWW.PORTACOCO.COM      "
            FCB     0

* --- Alt ROM loader placeholder ---
STR_ROM_T   FCC     "     *** ALT ROM LOADER ***     "
            FCB     0
STR_ROM_MSG FCC     "  USE OPTION [7] FLASH ROM MGMT "
            FCB     0

* --- DIAGNOSTICS STRINGS ---
STR_DIAG_T  FCC     "  *** SNIFFER DIAGNOSTICS ***   "
            FCB     0
STR_DIAG_H  FCC     "  -------RP2350 HARDWARE--------"
            FCB     0
STR_DIAG_1  FCC     "   CORE 1 PIO0 : ACTIVE (SNIFF) "
            FCB     0
STR_DIAG_2  FCC     "   CORE 1 PIO1 : IDLE           "
            FCB     0
STR_DIAG_3  FCC     "   DMA CH 0    : ACTIVE (AUDIO) "
            FCB     0
STR_DIAG_4  FCC     "   DMA CH 1    : IDLE           "
            FCB     0
STR_DIAG_5  FCC     "   BUS ERRORS  : 0              "
            FCB     0
STR_DIAG_R  FCC     "   "
            FCB     $01
            FCC     "[R]"
            FCB     $01
            FCC     " RESET ERROR COUNTERS     "
            FCB     0

* --- BRIDGE STATUS STRINGS ---
STR_BRG_T   FCC     "  *** ESP32 BRIDGE STATUS ***   "
            FCB     0
STR_BRG_H   FCC     "  -------SPI LINK METRICS-------"
            FCB     0
STR_BRG_1   FCC     "   LINK STATE  : CONNECTED      "
            FCB     0
STR_BRG_2   FCC     "   SPI CLOCK   : 10 MHz         "
            FCB     0
STR_BRG_3   FCC     "   TX PACKETS  : 4521           "
            FCB     0
STR_BRG_4   FCC     "   RX PACKETS  : 4519           "
            FCB     0
STR_BRG_5   FCC     "   DROPPED     : 0              "
            FCB     0
STR_BRG_P   FCC     "   "
            FCB     $01
            FCC     "[P]"
            FCB     $01
            FCC     " PING ESP32 BRIDGE        "
            FCB     0

* --- CLEAR WIFI STRINGS ---
STR_CLR_T   FCC     "  *** CLEAR WIFI SETTINGS ***   "
            FCB     0
STR_CLR_H   FCC     "  -----------WARNING!-----------"
            FCB     0
STR_CLR_1   FCC     "   THIS WILL ERASE YOUR SAVED   "
            FCB     0
STR_CLR_2   FCC     "   WIFI NETWORK AND PASSWORD    "
            FCB     0
STR_CLR_3   FCC     "   FROM THE ESP32 FLASH MEMORY. "
            FCB     0
STR_CLR_4   FCC     "   YOU WILL NEED TO RE-JOIN     "
            FCB     0
STR_CLR_5   FCC     "   A NETWORK AFTERWARDS.        "
            FCB     0
STR_CLR_Y   FCC     "   "
            FCB     $01
            FCC     "[Y]"
            FCB     $01
            FCC     " TO CONFIRM ERASE         "
            FCB     0
STR_CLR_X   FCC     "   "
            FCB     $01
            FCC     "[X]"
            FCB     $01
            FCC     " TO CANCEL/RETURN         "
            FCB     0

* --- TIME ZONE STRINGS ---
STR_TZ_T1   FCC     " *** TZ: NORTH AMERICA (1/3) ***"
            FCB     0
STR_TZ_T2   FCC     " *** TZ: EUROPE/WORLD  (2/3) ***"
            FCB     0
STR_TZ_T3   FCC     " *** TZ: ASIA/OCEANIA  (3/3) ***"
            FCB     0
STR_TZ_H    FCC     "  ------CURRENT SELECTION-------"
            FCB     0
STR_TZ_SEL  FCC     "       SELECT TIME ZONE:        "
            FCB     0
STR_TZ_1    FCC     "   "
            FCB     $01
            FCC     "[1]"
            FCB     $01
            FCC     " NFLD (UTC-3:30) ST JOHN'S"
            FCB     0
STR_TZ_2    FCC     "   "
            FCB     $01
            FCC     "[2]"
            FCB     $01
            FCC     " AST  (UTC-4) HALIFAX     "
            FCB     0
STR_TZ_3    FCC     "   "
            FCB     $01
            FCC     "[3]"
            FCB     $01
            FCC     " EST  (UTC-5) TORONTO     "
            FCB     0
STR_TZ_4    FCC     "   "
            FCB     $01
            FCC     "[4]"
            FCB     $01
            FCC     " CST  (UTC-6) WINNIPEG    "
            FCB     0
STR_TZ_5    FCC     "   "
            FCB     $01
            FCC     "[5]"
            FCB     $01
            FCC     " MST  (UTC-7) CALGARY     "
            FCB     0
STR_TZ_6    FCC     "   "
            FCB     $01
            FCC     "[6]"
            FCB     $01
            FCC     " PST  (UTC-8) VANCOUVER   "
            FCB     0
STR_TZ_7    FCC     "   "
            FCB     $01
            FCC     "[1]"
            FCB     $01
            FCC     " UTC  (GMT)               "
            FCB     0
STR_TZ_8    FCC     "   "
            FCB     $01
            FCC     "[2]"
            FCB     $01
            FCC     " WET  (UTC+0) LONDON      "
            FCB     0
STR_TZ_9    FCC     "   "
            FCB     $01
            FCC     "[3]"
            FCB     $01
            FCC     " CET  (UTC+1) PARIS       "
            FCB     0
STR_TZ_10   FCC     "   "
            FCB     $01
            FCC     "[4]"
            FCB     $01
            FCC     " EET  (UTC+2) ATHENS      "
            FCB     0
STR_TZ_11   FCC     "   "
            FCB     $01
            FCC     "[5]"
            FCB     $01
            FCC     " MSK  (UTC+3) MOSCOW      "
            FCB     0
STR_TZ_12   FCC     "   "
            FCB     $01
            FCC     "[6]"
            FCB     $01
            FCC     " GST  (UTC+4) DUBAI       "
            FCB     0
STR_TZ_13   FCC     "   "
            FCB     $01
            FCC     "[1]"
            FCB     $01
            FCC     " IST  (UTC+5:30) NEW DELHI"
            FCB     0
STR_TZ_14   FCC     "   "
            FCB     $01
            FCC     "[2]"
            FCB     $01
            FCC     " CST  (UTC+8) BEIJING     "
            FCB     0
STR_TZ_15   FCC     "   "
            FCB     $01
            FCC     "[3]"
            FCB     $01
            FCC     " JST  (UTC+9) TOKYO       "
            FCB     0
STR_TZ_16   FCC     "   "
            FCB     $01
            FCC     "[4]"
            FCB     $01
            FCC     " AWST (UTC+8) PERTH       "
            FCB     0
STR_TZ_17   FCC     "   "
            FCB     $01
            FCC     "[5]"
            FCB     $01
            FCC     " ACST (UTC+9:30) ADELAIDE "
            FCB     0
STR_TZ_18   FCC     "   "
            FCB     $01
            FCC     "[6]"
            FCB     $01
            FCC     " AEST (UTC+10) SYDNEY     "
            FCB     0

STR_RTN_X   FCC     "    PRESS "
            FCB     $01
            FCC     "[X]"
            FCB     $01
            FCC     " TO RETURN         "
            FCB     0
STR_RTN_N   FCC     "       "
            FCB     $01
            FCC     "[N]"
            FCB     $01
            FCC     "EXT "
            FCB     $01
            FCC     "[X]"
            FCB     $01
            FCC     " RETURN        "
            FCB     0
STR_RTN_P   FCC     "       "
            FCB     $01
            FCC     "[P]"
            FCB     $01
            FCC     "REV "
            FCB     $01
            FCC     "[X]"
            FCB     $01
            FCC     " RETURN        "
            FCB     0
STR_RTN_NP  FCC     "    "
            FCB     $01
            FCC     "[P]"
            FCB     $01
            FCC     "REV "
            FCB     $01
            FCC     "[N]"
            FCB     $01
            FCC     "EXT "
            FCB     $01
            FCC     "[X]"
            FCB     $01
            FCC     " RETURN    "
            FCB     0
STR_TZ_PROMPT FCC   "         SELECT 1-6 OR         "
            FCB     0
STR_SAVING  FCC     "  *** SAVING CONFIGURATION ***  "
            FCB     0
STR_SAVE_SUB FCC    "  SAVING THESE SETTINGS TO FLASH:"
            FCB     0
STR_SAVE_LA FCC     " AUDIO: "
            FCB     0
STR_SAVE_LV FCC     " VIDEO: "
            FCB     0
STR_SAVE_LC FCC     " COMM : "
            FCB     0
STR_SAVE_LD FCC     " DISK : "
            FCB     0
STR_SAVE_LR FCC     " RTC  : "
            FCB     0
STR_SAVE_LT FCC     " TZ   : "
            FCB     0
STR_SAVE_CONT FCC   "   PRESS ANY KEY TO CONTINUE    "
            FCB     0

TZ_NAME_TAB FDB     STR_TZN_1
            FDB     STR_TZN_2
            FDB     STR_TZN_3
            FDB     STR_TZN_4
            FDB     STR_TZN_5
            FDB     STR_TZN_6
            FDB     STR_TZN_7
            FDB     STR_TZN_8
            FDB     STR_TZN_9
            FDB     STR_TZN_10
            FDB     STR_TZN_11
            FDB     STR_TZN_12
            FDB     STR_TZN_13
            FDB     STR_TZN_14
            FDB     STR_TZN_15
            FDB     STR_TZN_16
            FDB     STR_TZN_17
            FDB     STR_TZN_18

STR_TZN_1   FCC     "NFLD"
            FCB     0
STR_TZN_2   FCC     "NST"
            FCB     0
STR_TZN_3   FCC     "EST"
            FCB     0
STR_TZN_4   FCC     "CST"
            FCB     0
STR_TZN_5   FCC     "MST"
            FCB     0
STR_TZN_6   FCC     "PST"
            FCB     0
STR_TZN_7   FCC     "AKST"
            FCB     0
STR_TZN_8   FCC     "HST"
            FCB     0
STR_TZN_9   FCC     "WET"
            FCB     0
STR_TZN_10  FCC     "CET"
            FCB     0
STR_TZN_11  FCC     "EET"
            FCB     0
STR_TZN_12  FCC     "MSK"
            FCB     0
STR_TZN_13  FCC     "IST"
            FCB     0
STR_TZN_14  FCC     "CHINA"
            FCB     0
STR_TZN_15  FCC     "JST"
            FCB     0
STR_TZN_16  FCC     "AEST"
            FCB     0
STR_TZN_17  FCC     "NZST"
            FCB     0
STR_TZN_18  FCC     "SST"
            FCB     0

TZ_STR_TAB  FDB     STR_TZ_1
            FDB     STR_TZ_2
            FDB     STR_TZ_3
            FDB     STR_TZ_4
            FDB     STR_TZ_5
            FDB     STR_TZ_6
            FDB     STR_TZ_7
            FDB     STR_TZ_8
            FDB     STR_TZ_9
            FDB     STR_TZ_10
            FDB     STR_TZ_11
            FDB     STR_TZ_12
            FDB     STR_TZ_13
            FDB     STR_TZ_14
            FDB     STR_TZ_15
            FDB     STR_TZ_16
            FDB     STR_TZ_17
            FDB     STR_TZ_18

            END     START
