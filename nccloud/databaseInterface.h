/*
 * databaseInterface.h
 *
 *  Created on: 2017年4月9日
 *      Author: root
 */

#ifndef DATABASEINTERFACE_H_
#define DATABASEINTERFACE_H_

#include <string>
#include <vector>
#include <mysql.h>
#include <thread>
#include <mutex>
#define ERROR_QUERY_FAIL -1 // 操作失败

using namespace std;
//std::mutex m;
// 定义MySQL连接信息
typedef struct
{
    char* server;
    char* user;
    char* password;
    char* database;
    int port;
}MySQLConInfo;

class MySQLInterface
{
public:

	static MySQLInterface *GetInstance()
	{
		if(SingleInstance==nullptr)
		{
			//m.lock();
			if(SingleInstance==nullptr)
			{
				SingleInstance = new MySQLInterface();
			}
			//m.unlock();
		}
		return SingleInstance;
	}

	static void Destory_MySQLInterface()
	{
		if(SingleInstance!=NULL)
		{
			delete SingleInstance;
			SingleInstance = nullptr;
		}
	}


    MySQLInterface();
    virtual ~MySQLInterface();

    void SetMySQLConInfo(char* server, char* username, char* password, char* database, int port);// 设置连接信息
    bool Open();  // 打开连接
    void Close(); // 关闭连接

    bool Select(const std::string& Querystr, std::vector<std::vector<std::string> >& data);      // 读取数据
    bool Update(const std::string& Updatestr); //更新数据
    bool Query(const std::string& Querystr);     // 其他操作
    int GetInsertID(const std::string& Querystr);// 插入并获取插入的ID,针对自动递增ID
    void ErrorIntoMySQL();       // 错误消息

public:
    int ErrorNum;                // 错误代号
    const char* ErrorInfo;       // 错误提示

private:
    MySQLConInfo MysqlConInfo;   // 连接信息
    static MySQLInterface *SingleInstance;         // MySQL对象
    MYSQL MysqlInstance;
    MYSQL_RES *Result;           // 用于存放结果
};






#endif /* DATABASEINTERFACE_H_ */
