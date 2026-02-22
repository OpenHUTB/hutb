
# 列出git软件目录下的所有文件，供.spec文件的datas参数使用
import os

def get_all_files(directory):
    file_list = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            file_list.append(file_path)
    return file_list

def get_package_str():
    
    # 获取所有文件
    all_files = get_all_files('git')
    
    # 打印所有文件路径
    package_lists = []
    idx = 0
    for file in all_files:
        idx += 1
        print(file)
        if idx > 1:
            package_lists.append(',')
        package_lists.append('("' + file + '", "' + os.path.dirname(file) + '\\")')

    print(len(all_files), "files found in 'git' directory.")
    # 将列表合并成字符串
    package_str=''.join(package_lists)
    print(package_str)
    # 单个反斜杠在字符串中需要转义，所以使用双反斜杠
    package_str = package_str.replace('\\', '\\\\')

    # 这里生成的字符串需要手动拷贝到Util\hutb_downloader.spec文件的Analysis对象的datas参数中
    with open('git_files.txt', 'w') as f:
        f.write(package_str)
    
    # 直接通过传递 --add-data 参数会导致命令过长，
    # 原因：一般来说，Windows命令行的最大长度约为8191个字符（Windows 10以后版本），但在某些情况下可能会更短
    # cmd_str = 'Pyinstaller download_from_git.py --onedir ' + package_str + ' -i hutb_log.ico --name hutb_downloader'


if __name__ == "__main__":

    package_str = get_package_str()
    # print(package_str)
