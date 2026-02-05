# 从git下载发行版
# 参考：https://cloud.tencent.com/developer/article/1600803
# 
# 关闭代理
# pip3 install gitpython
# 文档：https://gitpython.readthedocs.io/en/stable/reference.html
# 
# 打包成exe
# pip install pyinstaller
# -i "icon.ico"  指定图标
# -F 单文件模式
# Pyinstaller -F download_from_git.py --name hutb_downloader.exe
# 不被杀毒软件误报：https://blog.csdn.net/eastdawnc/article/details/113813790
# Pyinstaller download_from_git.py --onefile --name hutb_downloader.exe

import argparse
import datetime
import os, stat
import shutil
import sys
import time
import zipfile

import git
from git.repo import Repo
from git.repo.fun import is_git_dir


class Progress(git.remote.RemoteProgress):
    def update(self, op_code, cur_count, max_count=None, message=""):
        print(
            'Download (operation code: %s, current count: %s, max count: %s, percentage: %s, message: %s)...'%(
                op_code,
                cur_count,
                max_count,
                cur_count / (max_count or 100.0),
                message or "NO MESSAGE",
            )
        )


class GitRepository(object):
    """
    git仓库管理
    """

    def __init__(self, local_path, repo_url, branch='master'):
        self.local_path = local_path
        self.repo_url = repo_url
        self.repo = None
        self.initial(repo_url, branch)

    def initial(self, repo_url, branch):
        """
        初始化git仓库
        :param repo_url:
        :param branch:
        :return:
        """
        if not os.path.exists(self.local_path):
            os.makedirs(self.local_path)

        git_local_path = os.path.join(self.local_path, '.git')
        if not is_git_dir(git_local_path):
            # TODO: Progress bar while fetching files from git-lfs
            self.repo = Repo.clone_from(repo_url, to_path=self.local_path, branch=branch, progress=Progress(), depth=1)
        else:
            self.repo = Repo(self.local_path)

    def reset_hard(self):
        """
        重置仓库到最新状态
        :return:
        """
        print("Resetting repository to latest state...")
        self.repo.git.reset('--hard', 'HEAD~1')

    def commit_push_force(self, message="Update release files"):
        """
        强制提交所有更改
        :return:
        """
        # 判断仓库是否有更改
        if self.repo.is_dirty(untracked_files=True):
            print("Committing and pushing changes...")
            self.repo.git.add(A=True)
            self.repo.git.commit('-m', message)
        # 需要手动去除掉远程仓库的保护分支，才能强制推送
        self.repo.git.push(force=True)


    def pull(self):
        """
        从线上拉最新代码
        :return:
        """
        self.repo.git.pull()

    def branches(self):
        """
        获取所有分支
        :return:
        """
        branches = self.repo.remote().refs
        return [item.remote_head for item in branches if item.remote_head not in ['HEAD', ]]

    def commits(self):
        """
        获取所有提交记录
        :return:
        """
        commit_log = self.repo.git.log('--pretty={"commit":"%h","author":"%an","summary":"%s","date":"%cd"}',
                                       max_count=50,
                                       date='format:%Y-%m-%d %H:%M')
        log_list = commit_log.split("\n")
        return [eval(item) for item in log_list]

    def tags(self):
        """
        获取所有tag
        :return:
        """
        return [tag.name for tag in self.repo.tags]

    def change_to_branch(self, branch):
        """
        切换分值
        :param branch:
        :return:
        """
        self.repo.git.checkout(branch)

    def change_to_commit(self, branch, commit):
        """
        切换commit
        :param branch:
        :param commit:
        :return:
        """
        self.change_to_branch(branch=branch)
        self.repo.git.reset('--hard', commit)

    def change_to_tag(self, tag):
        """
        切换tag
        :param tag:
        :return:
        """
        self.repo.git.checkout(tag)
    

def split_file(input_file, output_dir, chunk_size):
    with open(input_file, 'rb') as f:
        index = 0
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            # 按0对齐为了保证合并时也是按顺序进行合并
            output_file = os.path.join(output_dir, f'hutb_{index:05d}.dat')
            print("Creating file: ", output_file)
            with open(output_file, 'wb') as out:
                out.write(chunk)
            index += 1

def merge_files(input_dir, output_file):
    files = os.listdir(input_dir)
    files.sort()
    
    with open(output_file, 'wb') as out:
        for file_name in files:
            print("Merging file: ", file_name)
            file_path = os.path.join(input_dir, file_name)
            with open(file_path, 'rb') as f:
                chunk = f.read()
                out.write(chunk)


# 解决 shutil.rmtree 删除.git文件夹中的文件，出现 PermissionError（只读文件无法删除）的问题
def remove_readonly(func, path, _):
    "Clear the readonly bit and reattempt the removal"
    os.chmod(path, stat.S_IWRITE)
    func(path)


# 杀死占用CarlaUE4.exe进程（默认2000端口）
def kill_process_on_port(port):
    kill_command = "for /f \"tokens=5\" %%a in (\'netstat -ano ^| findstr :%d\') do taskkill /F /PID %%a" % port
    os.system(kill_command)


if __name__ == '__main__':

    argparser = argparse.ArgumentParser(
        description=__doc__)
    argparser.add_argument(
        '-r',
        '--repository',
        metavar='R',
        default='release',
        help='HUTB big file repository (release, dependencies, Content) to download (default: release)')
    argparser.add_argument(
        '-u',
        '--upload',
        metavar='U',
        default='none',
        help='Upload to big file repository (release, dependencies, Content) (default: none means no upload, only download)')
    args = argparser.parse_args()

    start = datetime.datetime.now()

    remote_path = f"https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/{args.repository}.git"

    # 如果指定 -u 参数（上传发行版），则 -r 参数无效
    # d:\hutb\Build\dependencies\prerequisites\miniconda3\envs\hutb\python.exe download_from_git.py -u release
    if args.upload == 'release':
        print("Upload to repository: %s" % args.upload)
        # 获取当前代码路径的上级目录
        parent_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
        local_path = os.path.join(parent_dir, 'Build', 'UE4Carla')  # , 'hutb'
        rep_path = os.path.join(local_path, 'release')
        print("Upload path: ", rep_path)
        repo = GitRepository(rep_path, remote_path)
        # 寻找 local_path 目录下修改日期最新的 .zip 文件
        latest_file = None
        latest_file_name = None
        latest_time = 0
        for file_name in os.listdir(local_path):
            if file_name.endswith('.zip'):
                file_path = os.path.join(local_path, file_name)
                file_time = os.path.getmtime(file_path)
                if file_time > latest_time:
                    latest_time = file_time
                    latest_file = file_path
                    latest_file_name = file_name
        print("Latest zip file to upload: ", latest_file)
        # 如果提交的记录数超过2，则重置仓库到最新状态，避免提交记录过多
        if len(repo.commits()) > 2:
            repo.reset_hard()
        
        # 删除 rep_path 下的所有 .dat 文件
        for file_name in os.listdir(rep_path):
            if file_name.endswith('.dat'):
                file_path = os.path.join(rep_path, file_name)
                print("Removing file: ", file_path)
                os.remove(file_path)
        # 将最新的 .zip 重命名为 hutb.zip，并切分成多个 .dat 小文件，上传到 git 仓库中
        if os.path.exists( os.path.join(local_path, latest_file) ):
            os.rename(os.path.join(local_path, latest_file), os.path.join(local_path, 'hutb.zip'))    
        if os.path.exists( os.path.join(rep_path, '*.dat') ) is False:
            split_file( os.path.join(local_path, 'hutb.zip') , 
                    rep_path, 
                    256 * 1024 * 1024)  # 每个小文件最大为 256MB
        repo.commit_push_force(message=latest_file_name)
        print("Upload finished.")

        sys.exit(0)
    # download_from_git.py -u upload
    # 退出程序（用于调试）
    # sys.exit(0)

    print("Repository to download: %s" % args.repository)
    # 获取当前代码路径的上级目录
    # os.path.dirname(__file__) 打包成exe后，会下载到系统的临时文件夹中（比如：C:\Users\nongf\AppData\Local\Temp\_MEI197442\hutb）
    cur_dir = os.getcwd()
    if args.repository == 'release':
        save_dir = 'hutb'
    else:
        save_dir = args.repository
    local_path = os.path.join(cur_dir, save_dir)  # , 'hutb'
    print("Download path: ", local_path)
    
    if os.path.exists( local_path ):
        # Remove previous download folder
        shutil.rmtree( local_path , onerror=remove_readonly)
    repo = GitRepository(local_path, remote_path)

    # 移除工程中不相关的文件
    print("Remove .git folder and .gitattributes file...")
    if os.path.exists( os.path.join(local_path, '.git') ):
        shutil.rmtree( os.path.join(local_path, '.git') , onerror=remove_readonly)
        os.remove( os.path.join(local_path, '.gitattributes') )
    
    # 只有发行版仓库才需要进行合并小文件、解压、删除小文件的操作
    if args.repository == 'release':
        # 合并小文件
        print("Merge small files...")
        if os.path.exists( os.path.join(local_path, 'hutb.zip') ) is False:
            merge_files(local_path, os.path.join(local_path, 'hutb.zip'))

        print("Remove .dat small files...")
        # 先于解压删除后缀名为.dat的小文件，防止整个过程占用过多的磁盘空间
        for file_name in os.listdir(local_path):
            if file_name.endswith('.dat'):
                print("Removing file: ", file_name)
                os.remove( os.path.join(local_path, file_name) )

        print("Unzip hutb.zip...")
        f = zipfile.ZipFile(os.path.join(local_path, 'hutb.zip'), 'r') # 压缩文件位置
        for file in f.namelist():
            print("Extracting file: ", file)
            f.extract(file, local_path)               # 解压位置
        f.close()
        if os.path.exists( os.path.join(local_path, 'hutb.zip') ):
            print("Remove hutb.zip...")
            os.remove( os.path.join(local_path, 'hutb.zip') )


    cost_time = datetime.datetime.now() - start
    # 当网络带宽足够大时，下载时间大约4-5分钟左右
    print('Download finished, cost: %s' % (cost_time))
    print("Download to: ", local_path)
    kill_process_on_port(2000)  # 下载完成后自动启动CarlaUE4.exe，方便用户查看下载结果
    if os.path.exists( os.path.join(local_path, 'CarlaUE4.exe') ):
        os.system("start "" %s" % os.path.join(local_path, 'CarlaUE4.exe'))  # 启动CarlaUE4.exe
    time.sleep(15)  # 延时15秒，方便查看命令行输出
