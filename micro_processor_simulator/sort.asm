
ASSUME CS:CODE, DS:DATA

DATA SEGMENT
    LIST  DW 0125H,0144H,3001H,0003H,0002H
    COUNT EQU 05H
DATA ENDS

CODE SEGMENT
START:
    MOV  AX, DATA
    MOV  DS, AX

    MOV  DX, COUNT-1        ; outer passes
BACK:
    MOV  CX, DX             ; inner loop count
    MOV  SI, OFFSET LIST    ; start at LIST each pass
AGAIN:
    MOV  AX, [SI]
    CMP  AX, [SI+2]
    JC   GO                 ; if AX < next, no swap
    XCHG AX, [SI+2]
    XCHG AX, [SI]           ; swap [SI] and [SI+2]
GO:
    ADD  SI, 2
    LOOP AGAIN

    DEC  DX
    JNZ  BACK

    ; Proper DOS exit (instead of INT 03h)
    MOV  AX, 4C00h
    INT  21h
CODE ENDS
END START
