#!/usr/bin/env python3
#coding:utf-8


#请在nccloud目录下执行
import  os
import sys
from random import  Random


testFileDirPrefix = "/home/cai/testFiles/testFiles_"   #这是测试文件夹的前缀
filePrefix = "test"  #这是测试文件的前缀

nccloudDir = "/home/cai/degradedReadForEC1.0/degradedReadForEC/nccloud/bin/nccloud"  #执行文件目录
configFile = "/home/cai/degradedReadForEC1.0/degradedReadForEC/nccloud/config_local " #配置文件目录
nccloudData = "/home/cai/degradedReadForEC1.0/degradedReadForEC/nccloud/store"
flag = 1 #1表示一次性执行 0表示单独执行

deleteDirPrefix = "/home/cai/testFiles"


def random_str(randomLength):
    str=''
    chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
    length = len(chars)-1
    random = Random()
    for i in range(randomLength):
        str+=chars[random.randint(0,length)] #random.randint用于生成一个制定范围内的整数 其中参数a是下限 b是上限 都是闭区间
    print(str)
    return str

def saveStringToFile(filename,contents):
    fh = open(filename,'w')
    fh.write(contents)
    fh.close()

def randow_str_to_file(filename,randomLength = 8):
    str = random_str(randomLength)
    saveStringToFile(filename,str)




def execute(operation,fileSize,fileNumber):
    print operation
    nccloudCommand = nccloudDir+" "+configFile+" "+operation+" " #除了文件名称的执行命令 注意空格隔开
    print nccloudCommand,fileNumber,fileSize
    commandNumber = 0
    while commandNumber<fileNumber:

        if(operation=="encode"):
            print flag
            if(flag==0):  #每个命令单个执行
                fileName = filePrefix+"%d_%s"%(commandNumber,fileSize)  #文件名称
                totalNccloudCommand = nccloudCommand+testFileDirPrefix+fileSize+'/'+fileName  #执行命令添加文件名称
                print(totalNccloudCommand)
                #os.system(totalNccloudCommand)

            if(flag==1): #拼接命令最后执行
              
                fileName = filePrefix+"%d_%s"%(commandNumber,fileSize)  #文件名称
                #print(fileName)
                nccloudCommand = nccloudCommand+testFileDirPrefix+fileSize+'/'+fileName+" " #不断添加文件名称 注意空格
                print (nccloudCommand)



        if(operation=="decode"):  #和encode类似
            if(flag==0):#每个命令逐步执行
                fileName = "test%d_%s"%(commandNumber,fileSize)
                totalNccloudCommand = nccloudCommand+fileName
                print (totalNccloudCommand)
                os.system(totalNccloudCommand)
            if(flag==1): #命令合成一个执行
                fileName = "test%d_%s"%(commandNumber,fileSize)
                #print(fileName)
                nccloudCommand = nccloudCommand+fileName+" "
                #print (nccloudCommand)



        if(operation=="delete"): #和encode类似
            if(flag==0):#每个命令逐步执行
                fileName = "test%d_%s"%(commandNumber,fileSize)
                totalNccloudCommand = nccloudCommand+fileName
                print (totalNccloudCommand)
                # os.system(totalNccloudCommand)
            if(flag==1): #命令合成一个执行
                fileName = "test%d_%s"%(commandNumber,fileSize)
                #print(fileName)
                nccloudCommand = nccloudCommand+fileName+" "
                #print (nccloudCommand)

        commandNumber = commandNumber+1

    if(flag==1):  #如果flag为1 在这里同意执行
        print (nccloudCommand)
        os.system(nccloudCommand)

    
def generateFile(number,fileSize):  #负责生成文件

    fileNumber = 0
    #创建文件的文件夹目录
    testFileDir = testFileDirPrefix+fileSize+"/"
    if(os.path.exists(testFileDir)==False): #如果不存在创建目录
        os.makedirs(testFileDir)
    if(os.path.exists(testFileDir)==True):
        while fileNumber<number:   #创建的文件个数
            testFileName = filePrefix+str(fileNumber)+"_"+fileSize  #创建的文件名称 例如test1_4K 名称从0开始
            command =  "dd if=/dev/zero of="+testFileDir+testFileName+" bs=%s count=1"%(fileSize)  #创建文件
            print(command)
            os.system(command) #执行命令
            fileNumber = fileNumber+1


def generateRandomFile(number,fileSize): #这里的fileSize是字符串不是数字
    realSize = 1024;  #默认写1k
    if(fileSize[len(fileSize)-1:] == 'k' or fileSize[len(fileSize)-1:] =='K'):
        realSize = int(fileSize[:(len(fileSize)-1)])*1024
        print(realSize)
    if(fileSize[len(fileSize)-1:] == 'm' or fileSize[len(fileSize)-1:] =='M'):
        realSize = int(fileSize[:(len(fileSize)-1)])*1024*1024
        print(realSize)
    if(realSize>1024*1024*40):
        return

    fileNumber = 0
    #创建文件的文件夹目录
    testFileDir = testFileDirPrefix+fileSize+"/"
    if(os.path.exists(testFileDir)==False): #如果不存在创建目录
        os.makedirs(testFileDir)
    if(os.path.exists(testFileDir)==True):
        while fileNumber<number:   #创建的文件个数
            testFileName = filePrefix+str(fileNumber)+"_"+fileSize  #创建的文件名称 例如test1_4K 名称从0开始
            #command =  "dd if=/dev/zero of="+testFileDir+testFileName+" bs=%s count=1"%(fileSize)  #创建文件
            #os.system(command) #执行命令
            testFileTotalName = testFileDir+testFileName
            randow_str_to_file(testFileTotalName,realSize)
            fileNumber = fileNumber+1

#print(nccloudCommand)

def traversalDirAndDeleteFiles(dirName): #递归删除指定路径下的所有文件 （除了隐藏文件以.开头）

    #只能删除指定目录下的文件
    if not ((dirName.startswith(deleteDirPrefix)) or (dirName.startswith(nccloudData))):
        return


    if (os.path.isdir(dirName)):

        for f in os.listdir(dirName):
            if not f.startswith('.'):
                filePath = os.path.join(dirName,f) #拼接成完整路径名称
                #print(filePath)
                if os.path.isfile(filePath):

                    os.remove(filePath)  #如果是文件 删除文件
                if os.path.isdir(filePath):
                    traversalDirAndDeleteFiles(filePath)#如果是目录 递归调用




def executeEnDecodeDelete(fileFlag,fileNumber,fileSize):

    if(fileFlag=='0'):
        generateFile(fileNumber,fileSize)
    
    if(fileFlag=='1'):
        generateRandomFile(fileNumber,fileSize) #这个size是4k 4M之类
    if(fileFlag!='0' and fileFlag!='1'):
        return
    execute("encode",fileSize,fileNumber)
    execute("decode",fileSize,fileNumber)
    traversalDirAndDeleteFiles(nccloudData)
    traversalDirAndDeleteFiles(deleteDirPrefix)

#模式0  用dd操作生成文件  ；  模式1  生成随机字符串到制定文件中
#可以输入 1个参数  python generateFiles.py DELETE_ALL  删除所有测试文件
#可以输入 3个参数  python generateFiles.py 0 5 4k 在模式0下 产生5个4k文件
#可以输入 4个参数  python generateFiles.py 0 encode 5 4k   指的是在模式0下 产生5个4k文件 然后进行编码操作  模式0可换为模式1
#可以输入 5个参数  python generateFiles.py 0 EXECUTE_ALL 5 4k  在模式0（dd）下，产生5个4k文件 进行编码 然后解码 最后删除
if __name__ == '__main__':

    fileNumber = 5
    fileSize = "4k"
    operation = "encode"
    fileFlag = 0

    if(len(sys.argv)==5 and sys.argv[2]!="EXECUTE_ALL"): #需要输入三个参数 第一个是操作（编码 解码 删除） 第二个是个数 第三个是文件大小
        fileFlag = sys.argv[1]
        operation = sys.argv[2]
        fileNumber = int(sys.argv[3])
        fileSize = sys.argv[4]
        print(sys.argv[0],sys.argv[1],sys.argv[2],sys.argv[3],sys.argv[4])

        if(fileNumber>200):
            pass
        #必须在参数都满足的情况下才执行
        elif(fileFlag=='0'):
            generateFile(fileNumber,fileSize)
            execute(operation,fileSize,fileNumber)
        elif(fileFlag=='1'):
            generateRandomFile(fileNumber,fileSize) #这个size暂时主要写成1024*1024等
            execute(operation,fileSize,fileNumber)

    elif(len(sys.argv)==4): #参数

        flag = sys.argv[1]
        fileNumber = int(sys.argv[2])
        fileSize = sys.argv[3]
        if(fileNumber>200):

            pass
        elif(fileFlag=='0'):
            generateFile(fileNumber,fileSize)
        elif(fileFlag=='1'):
            generateRandomFile(fileNumber,fileSize) #这个size暂时主要写成1024*1024等

    elif(len(sys.argv)==2 and sys.argv[1]=="DELETE_ALL"):
        traversalDirAndDeleteFiles(deleteDirPrefix)  #这个删除的是生成文件的目录之下的所有文件
    
    elif(len(sys.argv)==3 and sys.argv[1]=="DELETE_ALL" and sys.argv[2]=="NCCLOUD"):
        traversalDirAndDeleteFiles(nccloudData)

    elif(len(sys.argv)==5 and sys.argv[2]=="EXECUTE_ALL"):
        fileFlag = sys.argv[1]  #dd  或者 字符串
        #operation = sys.argv[2] decode encode .. 这里修改为EXECUTE_ALL
        fileNumber = int(sys.argv[3]) #文件个数
        fileSize = sys.argv[4]   #文件大小 
        print(sys.argv[0],sys.argv[1],sys.argv[2],sys.argv[3],sys.argv[4])

        if(fileNumber<=200):
            executeEnDecodeDelete(fileFlag,fileNumber,fileSize)
            



        
