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

//解析URL，解析出主机名，资源名
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

//使用Get请求，得到响应
bool GetHttpResponse(const string & url, char * &response, int &bytesRead){
	string host, resource;
	if (!ParseURL(url, host, resource)){
		cout << "Can not parse the url" << endl;
		return false;
	}

	//建立socket
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

	//建立服务器地址
	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(80);
	//sa.sin_addr.s_addr = inet_addr(host.c_str());
	memcpy(&sa.sin_addr, hp->h_addr, 4);

	//建立连接
	if (0 != connect(sock, (SOCKADDR*)&sa, sizeof(sa))){
		cout << "Can not connect: " << url << endl;
		closesocket(sock);
		return false;
	};

	//准备发送数据
	string request = "GET " + resource + " HTTP/1.1\r\nHost: " + host + 
		"\r\nConnection: close\r\n\r\n";
	cout << request << endl;

	//发送数据
	if (SOCKET_ERROR == send(sock, request.c_str(), request.size(), 0)){
		cout << "send error" << endl;
		closesocket(sock);
		return false;
	}

	//接收数据
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
			pageBuf = (char*)realloc(pageBuf, m_nContentLength);       //重新分配内存
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

//提取所有的URL以及图片URL
void HTMLParse(string & htmlResponse, vector<string> & imgurls)
{
	//找所有连接，加入queue中
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

	//找所有图片，加入imgurls中
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

//把URL转化为文件名
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

//下载图片到img文件夹
void DownLoadImg(vector<string> & imgurls, const string &url){

	if (imgurls.empty()) return;
	//生成保存该url下图片的文件夹
	string foldname = ToFileName(url);
	foldname = "./img/" + foldname;
	if (!CreateDirectory(foldname.c_str(), NULL))
		cout << "Can not create directory:" << foldname << endl;
	char *image;
	int byteRead;
	for (int i = 0; i<imgurls.size(); i++){
		//判断是否为图片，bmp，jgp，jpeg，gif 
		string str = imgurls[i];
		int pos = str.find_last_of(".");
		if (pos == string::npos)
			continue;
		else{
			string ext = str.substr(pos + 1, str.size() - pos - 1);
			if (ext != "bmp"&& ext != "jpg" && ext != "jpeg"&& ext != "gif"&&ext != "png")
				continue;
		}
		//下载其中的内容
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



//广度遍历
void BFS(const string & url){
	cout << depth << endl;
	if (depth > 20) return;
	depth++;
	char * response;
	int bytes;
	// 获取网页的相应，放入response中。
	if (!GetHttpResponse(url, response, bytes)){
		cout << "The url is wrong! ignore." << endl;
		return;
	}
	string httpResponse = response;
	free(response);
	string filename = ToFileName(url);
	ofstream ofile("./html/" + filename);
	if (ofile.is_open()){
		// 保存该网页的文本内容
		ofile << httpResponse << endl;
		ofile.close();
	}
	vector<string> imgurls;
	//解析该网页的所有图片链接，放入imgurls里面
	HTMLParse(httpResponse, imgurls);

	//下载所有的图片资源
	DownLoadImg(imgurls, url);
}

void main()
{
	//初始化socket，用于tcp网络连接
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){
		return;
	}

	// 创建文件夹，保存图片和网页文本文件
	CreateDirectory("./img", 0);
	CreateDirectory("./html", 0);

	// 遍历的起始地址
	string urlStart = "http://nba.hupu.com";

	// 使用广度遍历
	// 提取网页中的超链接放入hrefUrl中，提取图片链接，下载图片。
	BFS(urlStart);

	// 访问过的网址保存起来
	visitedUrl.insert(urlStart);

	while (hrefUrl.size() != 0){
		string url = hrefUrl.front();  // 从队列的最开始取出一个网址
		cout << url << endl;
		BFS(url);					   // 遍历提取出来的那个网页，找它里面的超链接网页放入hrefUrl，下载它里面的文本，图片
		hrefUrl.pop();                 // 遍历完之后，删除这个网址
	}
	WSACleanup();
	return;
}
