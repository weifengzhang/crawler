#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "winsock2.h"
#include <time.h>
#include <queue>
#include <hash_set>

#pragma comment(lib, "ws2_32.lib") 
using namespace std;

#define DEFAULT_PAGE_BUF_SIZE 1048576

queue<string> hrefUrl;
hash_set<string> visitedUrl;
hash_set<string> visitedImg;
int depth = 0;
int g_ImgCnt = 1;

//����URL������������������Դ��
bool ParseURL(const string & url, string & host, string & resource){
	if (url.size() > 2000) {
		return false;
	}
	auto pos1 = url.find("http://");
	if (pos1 == string::npos) pos1 = 0;
	else pos1 += strlen("http://");
	auto pos2 = url.find('/', pos1);
	if (pos2 == string::npos)
	{
		host = url.substr(pos1);
		resource = "/";
	}
	else
	{
		host = url.substr(pos1, pos2 - pos1);
		resource = url.substr(pos2);
	}
	return true;
}

//ʹ��Get���󣬵õ���Ӧ
bool GetHttpResponse(const string & url, char * &response, int &bytesRead){
	string host, resource;
	if (!ParseURL(url, host, resource)){
		cout << "Can not parse the url" << endl;
		return false;
	}

	//����socket
	struct hostent * hp = gethostbyname(host.c_str());
	if (hp == NULL){
		cout << "Can not find host address" << endl;
		return false;
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1 || sock == -2){
		cout << "Can not create sock." << endl;
		return false;
	}

	//������������ַ
	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(80);
	//sa.sin_addr.s_addr = inet_addr(host.c_str());
	memcpy(&sa.sin_addr, hp->h_addr, 4);

	//��������
	if (0 != connect(sock, (SOCKADDR*)&sa, sizeof(sa))){
		cout << "Can not connect: " << url << endl;
		closesocket(sock);
		return false;
	};

	//׼����������
	string request = "GET " + resource + " HTTP/1.1\r\nHost: " + host + 
		"\r\nConnection: close\r\n\r\n";
	cout << request << endl;

	//��������
	if (SOCKET_ERROR == send(sock, request.c_str(), request.size(), 0)){
		cout << "send error" << endl;
		closesocket(sock);
		return false;
	}

	//��������
	int m_nContentLength = DEFAULT_PAGE_BUF_SIZE;
	char *pageBuf = (char *)malloc(m_nContentLength);
	memset(pageBuf, 0, m_nContentLength);

	bytesRead = 0;
	int ret = 1;
	cout << "Read: ";
	while (ret > 0){
		ret = recv(sock, pageBuf + bytesRead, m_nContentLength - bytesRead, 0);

		if (ret > 0)
		{
			bytesRead += ret;
		}

		if (m_nContentLength - bytesRead<100){
			cout << "\nRealloc memorry" << endl;
			m_nContentLength *= 2;
			pageBuf = (char*)realloc(pageBuf, m_nContentLength);       //���·����ڴ�
		}
		cout << ret << " ";
	}
	cout << endl;

	pageBuf[bytesRead] = '\0';
	response = pageBuf;
	closesocket(sock);
	return true;
	//cout<< response <<endl;
}

//��ȡ���е�URL�Լ�ͼƬURL
void HTMLParse(string & htmlResponse, vector<string> & imgurls)
{
	//���������ӣ�����queue��
	string tag("href=\"");
	auto beg = htmlResponse.find(tag);
	ofstream ofile("url.txt", ios::app);
	while (beg != string::npos)
	{
		beg += tag.size();
		auto end = htmlResponse.find('\"', beg);
		if (end == string::npos) return;
		string url = htmlResponse.substr(beg, end - beg);
		if (visitedUrl.find(url) == visitedUrl.end())
		{
			visitedUrl.insert(url);
			ofile << url << endl;
			hrefUrl.push(url);
		}
		beg = htmlResponse.find(tag, end);
	}
	ofile << endl << endl;
	ofile.close();

	//������ͼƬ������imgurls��
	tag = "<img ";
	string att1("src=\"");
	//string att2("lazy-src=\"");
	beg = htmlResponse.find(tag);
	while (beg != string::npos)
	{
		beg += tag.size();
		beg = htmlResponse.find(att1, beg);
		if (beg == string::npos) return;
		beg += att1.size();
		auto end = htmlResponse.find('\"', beg);
		if (end == string::npos) return;
		string imgUrl = htmlResponse.substr(beg, end - beg);
		if (visitedImg.find(imgUrl) == visitedImg.end())
		{
			visitedImg.insert(imgUrl);
			imgurls.push_back(imgUrl);
		}
		beg = htmlResponse.find(tag, end);
	}
	cout << "end of Parse this html" << endl;
}

//��URLת��Ϊ�ļ���
string ToFileName(const string &url){
	string fileName;
	fileName.resize(url.size());
	int k = 0;
	for (int i = 0; i<(int)url.size(); i++){
		char ch = url[i];
		if (ch != '\\'&&ch != '/'&&ch != ':'&&ch != '*'&&ch != '?'&&ch != '"'&&ch != '<'&&ch != '>'&&ch != '|')
			fileName[k++] = ch;
	}
	return fileName.substr(0, k) + ".txt";
}

//����ͼƬ��img�ļ���
void DownLoadImg(vector<string> & imgurls, const string &url){

	if (imgurls.empty()) return;
	//���ɱ����url��ͼƬ���ļ���
	string foldname = ToFileName(url);
	foldname = "./img/" + foldname;
	if (!CreateDirectory(foldname.c_str(), NULL))
		cout << "Can not create directory:" << foldname << endl;
	char *image;
	int byteRead;
	for (int i = 0; i<imgurls.size(); i++){
		//�ж��Ƿ�ΪͼƬ��bmp��jgp��jpeg��gif 
		string str = imgurls[i];
		int pos = str.find_last_of(".");
		if (pos == string::npos)
			continue;
		else{
			string ext = str.substr(pos + 1, str.size() - pos - 1);
			if (ext != "bmp"&& ext != "jpg" && ext != "jpeg"&& ext != "gif"&&ext != "png")
				continue;
		}
		//�������е�����
		if (GetHttpResponse(imgurls[i], image, byteRead)){
			if (strlen(image) == 0) {
				continue;
			}
			const char *p = image;
			const char * pos = strstr(p, "\r\n\r\n") + strlen("\r\n\r\n");
			int index = imgurls[i].find_last_of("/");
			if (index != string::npos){
				string imgname = imgurls[i].substr(index, imgurls[i].size());
				ofstream ofile(foldname + imgname, ios::binary);
				if (!ofile.is_open())
					continue;
				cout << g_ImgCnt++ << foldname + imgname << endl;
				ofile.write(pos, byteRead - (pos - p));
				ofile.close();
			}
			free(image);
		}
	}
}



//��ȱ���
void BFS(const string & url){
	cout << depth << endl;
	if (depth > 20) return;
	depth++;
	char * response;
	int bytes;
	// ��ȡ��ҳ����Ӧ������response�С�
	if (!GetHttpResponse(url, response, bytes)){
		cout << "The url is wrong! ignore." << endl;
		return;
	}
	string httpResponse = response;
	free(response);
	string filename = ToFileName(url);
	ofstream ofile("./html/" + filename);
	if (ofile.is_open()){
		// �������ҳ���ı�����
		ofile << httpResponse << endl;
		ofile.close();
	}
	vector<string> imgurls;
	//��������ҳ������ͼƬ���ӣ�����imgurls����
	HTMLParse(httpResponse, imgurls);

	//�������е�ͼƬ��Դ
	DownLoadImg(imgurls, url);
}

void main()
{
	//��ʼ��socket������tcp��������
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){
		return;
	}

	// �����ļ��У�����ͼƬ����ҳ�ı��ļ�
	CreateDirectory("./img", 0);
	CreateDirectory("./html", 0);

	// ��������ʼ��ַ
	string urlStart = "http://nba.hupu.com";

	// ʹ�ù�ȱ���
	// ��ȡ��ҳ�еĳ����ӷ���hrefUrl�У���ȡͼƬ���ӣ�����ͼƬ��
	BFS(urlStart);

	// ���ʹ�����ַ��������
	visitedUrl.insert(urlStart);

	while (hrefUrl.size() != 0){
		string url = hrefUrl.front();  // �Ӷ��е��ʼȡ��һ����ַ
		cout << url << endl;
		BFS(url);					   // ������ȡ�������Ǹ���ҳ����������ĳ�������ҳ����hrefUrl��������������ı���ͼƬ
		hrefUrl.pop();                 // ������֮��ɾ�������ַ
	}
	WSACleanup();
	return;
}
