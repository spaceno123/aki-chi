// LoadRunner.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#define _CRT_SECURE_NO_WARNINGS

// 参考にした、シリアル通信部分のwebページ
// RS232C　シリアル通信 http://www.ys-labo.com/BCB/2007/070512%20RS232C%20zenpan.html

#include <iostream>
#include <windows.h>
#include <fileapi.h>

typedef struct {
    uint8_t sdb[16];
}SDB;

#define SDB_COMMAND_OFS   (0)
#define SDB_ADDRESS_OFS   (2)
#define SDB_FORMAT_OFS    (6)
#define SDB_DATACOUNT_OFS (7)
#define SDB_DATA_OFS      (11)
#define SDB_RESERVED_OFS  (15)
#define SDB_COMMAND_SIZ   (2)
#define SDB_ADDRESS_SIZ   (4)
#define SDB_FORMAT_SIZ    (1)
#define SDB_DATACOUNT_SIZ (4)
#define SDB_DATA_SIZ      (4)
#define SDB_RESERVED_SIZ  (1)
#define SDB_COMMAND_WRITE_FILE   (0x0404)
#define SDB_COMMAND_ERROR_STATUS (0x0505)
#define SDB_COMMAND_JUMP_ADDRESS (0x0b0b)
#define SDB_COMMAND_SET_BAUDRATE (0x0d0d)

void setSDB(SDB* p, int offset, int count, uint32_t data)
{
    while (count--)
    {
        p->sdb[offset++] = (data >> (count * 8)) & 0xff;
    }

    return;
}

void sendCommPort(HANDLE hcomm, uint8_t *pData, int size)
{
    BOOL result;
    result = ClearCommError(hcomm, nullptr, nullptr);
    if (result == FALSE)
    {
        std::cout << "ClearCommError failed !\n";
        return;
    }

    DWORD wsize;
    result = WriteFile(hcomm, pData, size, &wsize, nullptr);
    if ((result == FALSE) || (size != wsize))
    {
        std::cout << "WriteFile failed !\n";
        return;
    }

    return;
}

int rcveCommPort(HANDLE hcomm, uint8_t *buf, int size)
{
    BOOL result;
    DWORD rsize;

    result = ReadFile(hcomm, buf, size, &rsize, nullptr);

    return rsize;
}

void send_error_status(HANDLE hcomm, SDB* pSdb)
{
    memset(pSdb, 0, sizeof(SDB));
    setSDB(pSdb, SDB_COMMAND_OFS, SDB_COMMAND_SIZ, SDB_COMMAND_ERROR_STATUS);
    sendCommPort(hcomm, (uint8_t *)pSdb, sizeof(SDB));

    return;
}

void send_write_file(HANDLE hcomm, SDB* pSdb, uint32_t address, uint32_t size)
{
    memset(pSdb, 0, sizeof(SDB));
    setSDB(pSdb, SDB_COMMAND_OFS, SDB_COMMAND_SIZ, SDB_COMMAND_WRITE_FILE);
    setSDB(pSdb, SDB_ADDRESS_OFS, SDB_ADDRESS_SIZ, address);
    setSDB(pSdb, SDB_DATACOUNT_OFS, SDB_DATACOUNT_SIZ, size);
    sendCommPort(hcomm, (uint8_t*)pSdb, sizeof(SDB));

    return;
}

void send_data(HANDLE hcomm, uint32_t size, FILE* fp)
{
    while (size--)
    {
        uint8_t buf[1];
        buf[0] = fgetc(fp);
        sendCommPort(hcomm, buf, 1);
    }

    return;
}

void send_data(HANDLE hcomm, uint32_t size, uint8_t* dp)
{
    while (size)
    {
        uint32_t sendSize = size < 1024 ? size : 1024;

        sendCommPort(hcomm, dp, sendSize);
        dp += sendSize;
        size -= sendSize;
    }

    return;
}

void send_jump_address(HANDLE hcomm, SDB* pSdb, uint32_t address)
{
    memset(pSdb, 0, sizeof(SDB));
    setSDB(pSdb, SDB_COMMAND_OFS, SDB_COMMAND_SIZ, SDB_COMMAND_JUMP_ADDRESS);
    setSDB(pSdb, SDB_ADDRESS_OFS, SDB_ADDRESS_SIZ, address);
    sendCommPort(hcomm, (uint8_t*)pSdb, sizeof(SDB));

    return;
}

void recv_result(HANDLE hcomm)
{
    uint8_t buf[16];
    int rn = rcveCommPort(hcomm, buf, sizeof(buf));

    if (rn)
    {
        for (int i = 0; i < rn; i++)
        {
            fprintf(stdout, " %02x", buf[i]);
        }
        std::cout << "\n";
    }

    return;
}

uint32_t getFileSize(FILE* fp)
{
    uint32_t size = 0;
    long pos = ftell(fp);

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, pos, SEEK_SET);

    return size;
}

int LoadRunner(char* com, char* ladr, char *jadr, char* lfile)
{
    int ret = 0;
    wchar_t* scom = new wchar_t[sizeof(com)+1];
    uint32_t load_address = strtoul(ladr, nullptr, 0);
    uint32_t jump_address = strtoul(jadr, nullptr, 0);

    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, scom, sizeof(com) + 1, com, _TRUNCATE);

    std::cout << " Com Port : " << com << "\n";
    std::cout << " Load Address : 0x" << std::hex << load_address << "\n";
    std::cout << " Load FIle : " << lfile << "\n";

    HANDLE hComm;
    hComm = CreateFile(scom,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hComm == INVALID_HANDLE_VALUE)
    {
        std::cout << "Com Port Open Error !\n";
        return -1;
    }

    BOOL result;
    result = SetupComm(hComm, 1024, 1024);
    if (result == FALSE)
    {
        std::cout << "SetupComm failed !\n";
        CloseHandle(hComm);
        return -1;
    }

    result = PurgeComm(hComm, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    if (result == FALSE)
    {
        std::cout << "PurgeComm failed !\n";
        CloseHandle(hComm);
        return -1;
    }

    DCB dcb;
    dcb.DCBlength = sizeof(DCB);
    result = GetCommState(hComm, &dcb);
    if (result == FALSE)
    {
        std::cout << "GetDcbComm failed !\n";
        CloseHandle(hComm);
        return -1;
    }
    dcb.BaudRate = 115200;
    result = SetCommState(hComm, &dcb);
    if (result == FALSE)
    {
        std::cout << "SetDcbComm failed !\n";
        CloseHandle(hComm);
        return -1;
    }

    COMMTIMEOUTS timeout;
    timeout.ReadIntervalTimeout = 500;
    timeout.ReadTotalTimeoutMultiplier = 0;
    timeout.ReadTotalTimeoutConstant = 500;
    timeout.WriteTotalTimeoutMultiplier = 0;
    timeout.WriteTotalTimeoutConstant = 500;
    result = SetCommTimeouts(hComm, &timeout);
    if (result == FALSE)
    {
        std::cout << "SetCommTimeouts failed !\n";
        CloseHandle(hComm);
        return -1;
    }

    SDB sdb;
    send_error_status(hComm, &sdb);
    recv_result(hComm);   // 56 78 78 56 f0 f0 f0 f0

    FILE* fp = fopen(lfile, "rb");
    if (fp == 0)
    {
        std::cout << "load file open error !\n";
        CloseHandle(hComm);
        return -1;
    }
    uint32_t filesize = getFileSize(fp);
    std::cout << " load file size : " << std::dec << filesize << "\n";
    send_write_file(hComm, &sdb, load_address, filesize);
    send_data(hComm, filesize, fp);
    recv_result(hComm);   // 56 78 78 56 88 88 88 88

    send_jump_address(hComm, &sdb, jump_address);
    recv_result(hComm);   // 56 78 78 56

    fclose(fp);
    CloseHandle(hComm);

    return ret;
}

int main(int argc, char **argv)
{   // load-address jump-address load-file
    std::cout << "*** Load Runner ***\n";

    if (argc == 5)
    {
        return LoadRunner(argv[1], argv[2], argv[3], argv[4]);
    }
    else
    {
        std::cout << "usage:>LoadRunner com-port load-address ivt-address load-bin-file\n";
    }
    return -1;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
