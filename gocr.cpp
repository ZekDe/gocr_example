#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>
#include <vector>
#include <sstream>
#include <memory>
#include <numeric>

#include <chrono>
#include <thread>
#include <mutex>
#include <future>


#include <cstdlib>
#include <cstdarg>
#include <cerrno>
#include <cstring>
#include <cstdint>


void clearstdin(void);
void printerr(const char *format, ...);
void exitsys(const char *format, ...);
void copyImageFileToRam();
std::string getUserInputData();
std::string getGocrData(int threshold_gray, int m, char **argv);
double compareGocrAndInput(const std::string &input, const std::string &gocr);
std::vector<std::string> generateGocrOperationModes(std::vector<uint32_t> numbers);
std::vector<std::string> generateGocrThresholdGray();
std::vector<std::string> generateGocrDustSize();
std::vector<std::string> generateGocrSpaceWidth();
std::vector<std::string> generateGocrCertainty();
std::string exec_popen(const std::string& cmd);

using namespace std;
using namespace chrono;


typedef struct
{
	int threshold_gray{};// -l (0..255)
	/*
	 * -m 2: tanınmayan karakterler için veri tabanındaki karakterleri kullanacak(atla
	 * -m 4: on - metinlerin düzenini analiz etme veya bölme işlemlerini (zoning) gerçekleştirme yeteneğini etkinleştirir.
	 * -m 8: tanınmayan karakterlerin yerine ona benzer koymaması istenir $ yerine E getirmek gibi
	 * -m 16 üst üste binmiş karakterler varsa ayırıp sonuç üretmeye çalışmaması söylenir
	 * -m 32 bağlam düzeltmesi, yani çevredeki karakterlere bakarakta karar vermeye çalışır
	 * -m 64 karakter paketleme, l,I  gibi benzer karakterlerin motora birinin gönderilmesi.
	 * -m 130 veri tabanı genişlet, tanımlanamayanlar için kullanıcan istekte bulunur. gocr -m 130 -p ./database/ text1.pbm
	 * -m 256 tanıma motorunu kapatır.(-m 2 ile anlamlıdır)
	 */
	int operation_mode{};// bit pattern -m 4,8,16,32,64
	int dustsize{};// -d(-1 auto detect)
	int spacewith{};// -s(0 auto detect)
	int certainty{};// -a (0..100)
}gocr_attr_t;


/*
 * gocr için komut satırı argümanlarının max değerleri
 */
namespace
{
constexpr static int threshold_gray_max{210};
constexpr static int dustsize_max{90};
constexpr static int spacewith_max{5}; // kullanım dışı
constexpr static int  certainty_max{95};
}


int main(int argc, char **argv)
{
	//  if(argc != 1)
	//    exitsys("resim için dosya yolu bilgisi giriniz\n");

	copyImageFileToRam();

	string input = getUserInputData();
	std::cout << input << "\n";

	string image_path{"/mnt/ramdisk/image.png"};
	auto str = exec_popen("gocr -u \"*\" " + image_path);
	auto result_prev = compareGocrAndInput(input, str);
	cout << "goc default result: " << result_prev << "\n";

	auto opmodes 		= generateGocrOperationModes(vector<uint32_t>{4,8,16,32,64});
	auto thresholdgray 	= generateGocrThresholdGray();
	auto dustsize 		= generateGocrDustSize();
	//auto spacewidth 	= generateGocrSpaceWidth();
	auto certainty		= generateGocrCertainty();


	/* Kombinasyon bölümü*/
	std::vector<string> svec;
	gocr_attr_t best;
	//double result{};

	auto start = steady_clock::now();
	for (size_t i1 = 0; i1 < opmodes.size() && !opmodes[i1].empty(); ++i1)
		for (size_t i2 = 0; i2 < thresholdgray.size() && !thresholdgray[i2].empty(); ++i2)
			for (size_t i3 = 0; i3 < dustsize.size() && !dustsize[i3].empty(); ++i3)
				//for (size_t i4 = 0; i4 < spacewidth.size() && !spacewidth[i4].empty(); ++i4)
				for (size_t i5 = 0; i5 < certainty.size() && !certainty[i5].empty(); ++i5)
				{
					svec.push_back(opmodes[i1] + " " + thresholdgray[i2] + " " +
							dustsize[i3] + " " + /*spacewidth[i4] + " " +*/
							certainty[i5]);

//					cout << "gocr -u \"*\" " + svec.back() + " " + image_path << "\n";
					auto str = exec_popen("gocr -u \"*\" " + svec.back() + " " + image_path);
					if(double result = compareGocrAndInput(input, str); result > result_prev)
					{
						result_prev = result;

						best.operation_mode = stoi(opmodes[i1].substr(opmodes[i1].find(' ') + 1));
						best.threshold_gray = stoi(thresholdgray[i2].substr(thresholdgray[i2].find(' ') + 1));
						best.dustsize = stoi(dustsize[i3].substr(dustsize[i3].find(' ') + 1));
						best.certainty = stoi(certainty[i5].substr(certainty[i5].find(' ') + 1));

						cout << svec.back() << ": " << result_prev << "\n";
					}
				}
	auto end = steady_clock::now();
	minutes mins{ duration_cast<minutes> ((end - start) % 1h) };
	seconds sec{ duration_cast<seconds>((end - start) % 1min) };
	cout << "sure: \n" << mins.count() << " dakika - "<< sec.count() << " saniye\n";

	return 0;
}

void copyImageFileToRam()
{
	// Dizin oluşturma komutu
	if (system("sudo mkdir -p /mnt/ramdisk") == -1)
		exitsys("Failed to create /mnt/ramdisk\n");

	cout << "/mnt/ramdisk created\n";

	// tmpfs dosya sistemini bağlama komutu
	if (system("sudo mount -t tmpfs -o size=20M tmpfs /mnt/ramdisk") == -1)
		exitsys("Failed to mount tmpfs /mnt/ramdisk\n");

	cout << "tmpfs mounted\n";

	// Dosyayı kopyalama komutu
	if (system("cp image.png /mnt/ramdisk/image.png") == -1)
		exitsys("Failed to copy file to /mnt/ramdisk\n");

	cout << "image.png copied to /mnt/ramdisk\n";
}

std::vector<std::string> generateGocrOperationModes(std::vector<uint32_t> numbers)
{
	std::vector<uint32_t> opnums(numbers.size());
	uint32_t tmp=0;
	std::copy(numbers.begin(), numbers.end(), opnums.begin());

	for (size_t i = 0; i < numbers.size(); ++i)
	{
		for (size_t j = i + 1; j < numbers.size(); ++j)
		{

			for (size_t k = 0; k <= i; ++k)
				tmp |= numbers[k];

			tmp = tmp | numbers[j];
			opnums.push_back(tmp);
			tmp = 0;
		}
	}


	std::vector<std::string> result(opnums.size());

	for (uint32_t i = 0; i < opnums.size(); ++i)
		result[i] = "-m " + std::to_string(opnums[i]);

	return result;
}

std::vector<std::string> generateGocrThresholdGray()
{
	std::vector<std::string> result(55, "");

	for(uint16_t i = 0, j = 0; j < threshold_gray_max+1; ++i, j+=10)
		result[i] = "-l " + std::to_string(j);

	return result;
}

std::vector<std::string> generateGocrDustSize()
{
	std::vector<std::string> result(25, "");
	result[0] = "-d -1";
	for(uint16_t i = 1, j = 0; j < dustsize_max+1; ++i, j+=10)
		result[i] = "-d " + std::to_string(j);

	return result;
}

std::vector<std::string> generateGocrSpaceWidth()
{
	std::vector<std::string> result(15, "");

	for(uint16_t i = 0; i < spacewith_max+1; ++i)
		result[i] = "-s " + std::to_string(i);

	return result;
}

std::vector<std::string> generateGocrCertainty()
{
	std::vector<std::string> result(25, "");

	for(uint16_t i = 0, j = 0; j < certainty_max+1; ++i, j+=5)
		result[i] = "-a " + std::to_string(j);

	return result;
}

double compareGocrAndInput(const std::string &input, const std::string &gocr)
{
	int cnt_match = 0;

	for(unsigned int i = 0; i < input.length(); ++i)
		if(input[i] == gocr[i])
			++cnt_match;

	return (double)cnt_match / input.length();
}


std::string getGocrData(int threshold_gray, int m, char **argv)
{
	enum{BUFFSIZE=1024};
	FILE *fp;
	std::vector<char> gocr_buff(BUFFSIZE);
	std::ostringstream oss;

	oss << "gocr -u \"*\" -l " << threshold_gray << " -m " << m <<" Adsız.png";

	cout << oss.str() << "\n";
	fp = popen(oss.str().c_str(), "r");

	if (fp == NULL)
		exitsys("command cannot execute\n");

	int cnt = fread(gocr_buff.data(), 1, gocr_buff.capacity(), fp);
	pclose(fp);

	printf("gocr total read: %d bytes\n", cnt);
	//std::copy(gocr_buff.begin(), gocr_buff.end(), std::ostream_iterator<char>(std::cout));
	std::replace(gocr_buff.begin(), gocr_buff.end(), '\n', ' ');

	return std::string(gocr_buff.begin(), gocr_buff.end());
}

std::string exec_popen(const std::string& cmd)
{
	std::array<char, 256> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

	if (!pipe)
		throw std::runtime_error("popen() failed!");

	while (fgets(buffer.data(), buffer.size(), pipe.get()))
		result += buffer.data();

	//std::replace(result.begin(), result.end(), '\n', ' ');

	return result;
}

std::string getUserInputData()
{
	std::string line;
	std::string input_buff;

	std::cout << "Veri Girişi(Çıkmak için exit):\n";

	for(;;)
	{
		getline(std::cin, line);

		if (line == "exit")
			break;


		if (!line.empty() && line[line.length() - 1] == '\n')
			line.erase(line.length() - 1);

		input_buff += line + " ";
	}

	return input_buff;
}


void clearstdin(void)
{
	int ch;

	while ((ch = getchar()) != EOF && ch != '\n')
		;
}

void printerr(const char *format, ...)
{
	va_list va;

	va_start(va, format);

	fprintf(stderr, "error: ");
	vfprintf(stderr, format, va);

	va_end(va);
}

void exitsys(const char *format, ...)
{
	va_list va;

	va_start(va, format);

	vfprintf(stderr, format, va);
	fprintf(stderr, ": %s", strerror(errno));

	va_end(va);

	exit(EXIT_FAILURE);
}
