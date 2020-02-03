#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <intrin.h>
#include <cassert>

#define ENABLE_FIRST_TEST true
#define ENABLE_SECOND_TEST true
#define ENABLE_THIRD_TEST true
#define IsAllEnabled() (ENABLE_FIRST_TEST && ENABLE_SECOND_TEST && ENABLE_THIRD_TEST)

#pragma region First_Test
inline 
void
hex_to_ascii(
    uint8_t value,
    char* string
)
{
    int32_t i = 0;
    for (i = 7; i >= 0; i--) {
        /*
            С помощью AND'а мы так раз берем нужное значение (0 и 1), которое
            затем без каких-либо проблем можно сконвертировать в ASCII путем
            простого прибавления числа, равного '0' - символу нуля.
        */
        string[7 - i] = ((value >> i) & 1) + 0x30;   
    }
}

template
<typename type_t>
void
PrintBinary(
    type_t value
)
{
    const size_t valueSize = sizeof(type_t);
    char printedString[valueSize * 8 + 1] = {};
    type_t tempValue = value;
    uint8_t* tempByte = (uint8_t*)&tempValue;
    int32_t i = 0;

    /*
        Так как число изначально находится в little-endian - нам следует его
        переконвертировать в big-endian для более понятного людям отображения.
    */
    switch (valueSize) {
    case sizeof(uint64_t): tempValue = _byteswap_uint64(value); break;
    case sizeof(uint32_t): tempValue = _byteswap_ulong(value);  break;
    case sizeof(uint16_t): tempValue = _byteswap_ushort(value); break;
    default: break;
    }

    for (size_t i = 0; i < valueSize; i++) {
        hex_to_ascii(tempByte[i], &printedString[i * 8]);
    }

    printf("The %u-bit size value - %s\n", int32_t(sizeof(type_t) * 8), printedString);
}

#pragma endregion First_Test
#pragma region Second_Test
void
RemoveDups(
    char* string
)
{
    char previousSymbol = 0;
    char* firstString = (char*)string;
    char* secondString = (char*)string;

    while (*firstString) {        // пока не дойдем до нулевого символа - работаем
        char symbol = *(firstString++);
        while (*firstString == symbol) { firstString++; }  // скипаем всеидущие аналогичные символы
        *(secondString++) = symbol;
    }

    *secondString = 0x0;
}
#pragma endregion Second_Test
#pragma region Third_Test
#include <vector>
#include <string>
/*
    Что-то похожее есть в RIFF (wav, avi и т.д) файлах. Есть некий
    хедер, в котором в начале пишется "волшебное" слово, по которому
    можно определить формат, а дальше либо просто размер и дата, либо
    формат даты и после него размер и дата
*/
struct FileStructHeader {
    uint32_t magicWord = 0;
    uint32_t dataSize = 0; 
    void* data = nullptr;
};

// структуру ListNode модифицровать нельзя
struct ListNode {
    ListNode* prev = nullptr;
    ListNode* next = nullptr;
    ListNode* rand = nullptr; // указатель на произвольный элемент данного списка, либо NULL
	std::string data;
};

class CListManipulator {
private:
    size_t GetFSize(FILE* file)
    {
		fseek(file, 0L, SEEK_END);
		size_t fileSize = ftell(file);
		fseek(file, 0L, SEEK_SET);
        return fileSize;
    }

    size_t ReadData(void* pointer, size_t size, FILE* file)
    {
        return fread(pointer, 1, size, file);
    }

    void ResizeBuffer(size_t sizeToResize)
    {
        if (pData) free(pData);
        pData = malloc(sizeToResize);
    }

	bool CheckFormat(FileStructHeader* file)
	{
		return (file->magicWord != 0xFFDDFFDD || !file->dataSize);
	}

	void FreeLists()
	{
        ListNode* pList = head;
		while (pList) {
            ListNode* pTemp = pList->next;
			delete pList;
			pList = pTemp;
		}
        count = 0;
	}

    size_t GetEndFileSize(ListNode* pFirst)
    {
        size_t retSize = 0;
        ListNode* pTemp = pFirst;
        while (pTemp) {
            retSize += pTemp->data.size() + 8;
            pTemp = pTemp->next;
        }
        return retSize;
    }

	void SetToOutputSize()
	{
        int32_t tempSize = 0;
		if ((tempSize = GetEndFileSize(head)) != dataSize) {
			dataSize = tempSize;
			ResizeBuffer(dataSize);
		}
	}

    bool ParseFile()
    {
        int32_t readedData = dataSize;
        FileStructHeader* pFileStruct = (FileStructHeader*)pData;
        if (!pFileStruct) return false;

        FreeLists();
        head = new ListNode;
        ListNode* pList = head;
        
        while (pFileStruct && readedData) {
            if (!CheckFormat(pFileStruct)) break;

            /* Берем указатель на первый символ и читаем дату*/
            char* pRealData = (char*)&pFileStruct->data;
            pList->data.reserve(size_t(pFileStruct->dataSize) + 1);
            for (size_t i = 0; i < pFileStruct->dataSize; i++) {
                pList->data.push_back(pRealData[i]);
            }

            /* Пишем сколько осталось непропарсенный даты и сдвигаем указатель */
			readedData -= (pFileStruct->dataSize + 8);
            pFileStruct = (FileStructHeader*)((size_t)pRealData + (size_t)pFileStruct->dataSize);

            /* Если есть дата - аллоцируем новую ноду */
            if (readedData > 0) {
                pList->next = new ListNode;
                pList->next->prev = pList;
                pList = pList->next;
            }

            count++;
        }

        tail = pList;
    }

    bool PrepareData()
    {
        SetToOutputSize();

		FileStructHeader* pFileStruct = (FileStructHeader*)pData;
		ListNode* pList = head;
        int32_t needyToWriteSize = dataSize;
        if (!pData || !needyToWriteSize) return false;
        memset(pData, 0, dataSize);
        count = 0;

        while (pList && needyToWriteSize) {
            void* pToData = &pFileStruct->data;
            pFileStruct->magicWord = _byteswap_ulong(0xFFDDFFDD);   // необходимо перевести в big-endian при записи в файл, иначе неправильно считает хедер
            pFileStruct->dataSize = pList->data.size();
            memcpy(pToData, pList->data.data(), pFileStruct->dataSize);
            
            pList = pList->next;
            needyToWriteSize -= (pFileStruct->dataSize + 8);
			pFileStruct = (FileStructHeader*)((size_t)pToData + (size_t)pFileStruct->dataSize);
        }  

        count++;
    }

public:
    CListManipulator() : count(0), head(nullptr), tail(nullptr) {}
    ~CListManipulator()
    {
        if (pData) free(pData);
        FreeLists();
    }

    void Serialize(FILE* file) // сохранение в файл (файл открыт с помощью fopen(path, "wb"))
	{
        if (!PrepareData()) return;
        if (fwrite(pData, 1, dataSize, file) != dataSize) return;
        fflush(file);
    }

    void Deserialize(FILE* file) // загрузка из файла (файл открыт с помощью fopen(path, "rb"))
    {
        FileStructHeader tempArray = {};
        dataSize = GetFSize(file);
        if (!dataSize) return;

		if (ReadData(&tempArray, 8, file) != 8) return;
        if (!CheckFormat(&tempArray)) return;
        fseek(file, 0L, SEEK_SET);

        if (pData) free(pData);
		pData = malloc(dataSize);
        if (pData) {    // IntelliSense жалуется, если не поставить if
            memset(pData, 0, dataSize);
            if (ReadData(pData, dataSize, file) != dataSize) {
                free(pData);
                return;
            }
        }

        ParseFile();
    }

    void put_data(const char* data) {
        if (data) {
            ListNode* pNode = new ListNode;
            pNode->data.append(data);
            if (tail) {
                tail->next = pNode;
                pNode->prev = tail;
                tail = pNode;
            }
            else {
                head = pNode;
                tail = head;
            }
        }

        count++;
    }

    ListNode* first() { return head; }
    ListNode* end() { return tail; }
    int32_t get_count() { return count; }

private:
    int32_t count;
    int32_t dataSize = 0;
    ListNode* head;
    ListNode* tail;
    void* pData = nullptr;
};
#pragma endregion Third_Test
#pragma region Full_Test
void
FirstTest()
{
    if constexpr (IsAllEnabled()) printf("####################FIRST TEST####################\n");
	printf("first variable: ");
	PrintBinary(uint32_t(0x4D5A0000));
	printf("second variable: ");
	PrintBinary(uint32_t(0xFACEF00D));
	printf("third variable: ");
	PrintBinary(size_t(0xDEADBEEFDEADBEEF));
    if (IsAllEnabled()) printf("\n");
}

void 
SecondTest()
{
    if constexpr (IsAllEnabled()) printf("####################SECOND TEST####################\n");
    const char pFirstString[] = "AAA BBB AAA\n";
    const char pSecondString[] = "And I'll strike down upon thee";
    const char pThirdString[] = " with great vengeance and furious Anger those who attempt to poison and destroy\nmy brothers. And you will know My name is the Lord when I lay my vengeance upon thee\n";
	RemoveDups(const_cast<char*>(pFirstString));
	RemoveDups(const_cast<char*>(pSecondString));
	RemoveDups(const_cast<char*>(pThirdString));
    printf("%s%s%s\n", pFirstString, pSecondString, pThirdString);
}

void 
ThirdTest()
{
    if constexpr (IsAllEnabled()) printf("####################THIRD TEST####################\n");
    const char* pFilePath = "C:\\1\\te.txt";
	CListManipulator manipulator;
	manipulator.put_data("Hi");
	manipulator.put_data("I'm serialized data!");
	manipulator.put_data("Can you write more data?");
	manipulator.put_data("You can't!");
	manipulator.put_data("Just deal with that");
	manipulator.put_data("11111111111111111111111111111111111111111111111111111111111111111111111111111");
	printf("Manipulator data puted\n");

	FILE* pFile = nullptr;
	fopen_s(&pFile, pFilePath, "wb");
	if (!pFile) {
		printf("Can't open %s file to write:\n", pFilePath);
		return;
	}

	printf("File opened: %s\n", pFilePath);
	manipulator.Serialize(pFile);
    if (pFile) {
        fclose(pFile);
        pFile = nullptr;
    }

	fopen_s(&pFile, pFilePath, "rb");
	if (!pFile) {
        printf("Can't open %s file to read:\n", pFilePath);
        return;
	}

	int32_t i = 0;
	manipulator.Deserialize(pFile);
    if (pFile) fclose(pFile);

	ListNode* pList = manipulator.first();
	printf("Succesfully readed data: \n");
	printf("Readed strings: %d\n", manipulator.get_count());
	while (pList) {
		printf("%i string - %s\n", i, pList->data.c_str());
		pList = pList->next;
		i++;
	}
}
#pragma endregion Full_Test

int 
main()
{
    if constexpr (ENABLE_FIRST_TEST) {
        FirstTest();
    }

    if constexpr (ENABLE_SECOND_TEST) {
        SecondTest();
    }

    if constexpr (ENABLE_THIRD_TEST) {
        ThirdTest();
    }

    system("pause");
    return 0;
}
