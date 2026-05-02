# 用法
# 1. 下载模拟器：
# hutb_downloader.exe
# 2. 下载发行版：
# hutb_downloader.exe -r lfs_demo
# 3. 添加不删除.git记录的选项(--skipp, -s)
#
# 打包方法： 
# Pyinstaller download_from_git.py --onefile --name hutb_downloader -i hutb_log.ico
# 
# 
# 从git下载发行版
# 参考：https://cloud.tencent.com/developer/article/1600803
#
# 关闭代理
# pip3 install gitpython
# 文档：https://gitpython.readthedocs.io/en/stable/reference.html
#
# 打包成exe
# pip install pyinstaller
# ** 根据.spec文件生成exe文件 **:
# 1. 当前目录放有cc文件夹：https://gitee.com/OpenHUTB/sw/releases/download/up/git_min.zip
# 2. python git_files.py 生成git_files.txt文件，复制其中的内容到Util\hutb_downloader.spec文件的Analysis对象的datas参数中，
# 将git目录添加到datas参数中，换一台机器还是找不到git，转而从gitee下载git_min.zip，并解压到当前目录下，这样就可以避免在打包成exe后，找不到git可执行文件的问题
# 3. Pyinstaller download_from_git.py --onefile --name hutb_downloader -i hutb_log.ico
# 
#
# 新机器问题：fatal: unable to access 'https://git.code.tencent.com/OpenHUTB/release.git/': error setting certificate verify locations: CAfile: E:/Projects/OpenHUTB-mcp/mcp/git/mingw64/ssl/certs/ca-bundle.crt/etc/ssl/certs/ca-bundle.crt CApath: none
# 解决：git\bin\git.exe config --global http.sslVerify false
# 
# 解决：没有拉取大文件：
# 解决：git\bin\git.exe lfs install
# 
# 
# 其他（开发过程）：
# 使用.spec文件打包：Pyinstaller hutb_downloader.spec
# -i "icon.ico"  指定图标
# -F 单文件模式
# Pyinstaller -F download_from_git.py --name hutb_downloader.exe
# 不被杀毒软件误报：https://blog.csdn.net/eastdawnc/article/details/113813790
# 文件夹：
# Pyinstaller download_from_git.py --onedir --add-data "git\bin\*;git\bin\" --add-data "git\cmd\*;git\cmd\"  -i hutb_log.ico --name hutb_downloader
# 然后用 7zip 打包成一个自解压可执行文件
# 单个文件：
# Pyinstaller download_from_git.py --onefile --add-data "git\bin\bash.exe;git\bin\" --add-data "git\bin\git.exe;git\bin\"  -i hutb_log.ico --name hutb_downloader
#
#
# 0. 上传到远程服务器：
# python.exe download_from_git.py -u release


import argparse
import datetime
import inspect
import os
import shutil
import ssl
import stat
import sys
import subprocess
import urllib.request
import zipfile


# 解决：urllib.error.URLError: <urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed: unable to get local issuer certificate (_ssl.c:1017)>
# https://www.cnblogs.com/lxmtx/p/12929905.html
# 全局取消证书验证
ssl._create_default_https_context = ssl._create_unverified_context


# 获取当前代码路径的上级目录
home_dir = os.path.abspath(os.path.join(os.getcwd(), ".."))
# 获取当前脚本所在的路径
# script_dir = os.path.dirname(os.path.abspath(__file__))
# 会下载到系统的临时文件中（C:\Users\nongf\AppData\Local\Temp\_MEI598322\git\bin\git.exe）
# script_dir = os.path.dirname(os.getcwd())
# 获取绝对路径的父目录
script_dir = inspect.getfile(inspect.currentframe())
script_dir = os.path.dirname(os.path.abspath(script_dir))

# 使用当前 git 目录下的 git 可执行文件
# 判断git目录是否存在
prerequisites_dir = os.path.join(home_dir, "Build", "dependencies", "prerequisites")
if os.path.exists(os.path.join(prerequisites_dir, "git")) and not os.path.exists(os.path.join(script_dir, "git", "bin", "git.exe")):
    # 将git目录拷贝到当前脚本所在的路径下
    print("Copying git directory from prerequisites to current script directory...")
    shutil.copytree(
        os.path.join(prerequisites_dir, "git"), os.path.join(script_dir, "git")
    )
elif not os.path.exists(os.path.join(prerequisites_dir, "git")) and not os.path.exists(os.path.join(script_dir, "git", "bin", "git.exe")):
    # 从gitee下载git_min.zip，并解压到当前目录下
    print(
        "Git directory not found in prerequisites, download it from https://gitee.com/OpenHUTB/sw/releases/download/up/git_min.zip and extract it to %s"
        % script_dir
    )
    urllib.request.urlretrieve(
        "https://gitee.com/OpenHUTB/sw/releases/download/up/git_min.zip",
        os.path.join(script_dir, "git_min.zip"),
    )
    with zipfile.ZipFile(os.path.join(script_dir, "git_min.zip"), "r") as zip_ref:
        zip_ref.extractall(script_dir)
else:
    print("Git directory not found in prerequisites or current script directory.")


# 这样可以避免在打包成exe后，找不到git可执行文件的问题
git_exe = os.path.join(script_dir, "git", "bin", "git.exe")
print("Using git executable: ", git_exe)
os.environ["GIT_PYTHON_GIT_EXECUTABLE"] = git_exe

# 必须在设置GIT_PYTHON_GIT_EXECUTABLE环境变量之后，才能导入git库，否则git库会使用系统环境变量中默认的git路径，导致找不到git可执行文件的问题
from git.repo import Repo
from git.repo.fun import is_git_dir

disable_ssl_verify_command = "%s config --global http.sslVerify false" % os.path.join(script_dir, 'git', 'bin', 'git.exe')
print(disable_ssl_verify_command)
os.system(disable_ssl_verify_command)  # 解决新机器上git拉取代码时，出现的证书验证问题

# 将本地的 Git 配置设置为不验证远程仓库的 LFS 锁定状态，
# 解决 git lfs push -f 报错：
# stdout: 'Locking support detected on remote "origin". Consider enabling it with: git config lfs.https://git.code.tencent.com/OpenHUTB/release.git.locksverify false'
disable_lfs_lock_verify_command = "%s config lfs.https://git.code.tencent.com/OpenHUTB/release.git.locksverify false" % os.path.join(script_dir, 'git', 'bin', 'git.exe')
print(disable_lfs_lock_verify_command)
os.system(disable_lfs_lock_verify_command)


def show_progress_bar(current, total, bar_length=40):
    """显示进度条"""
    percentage = 100.0 if total == 0 else (current / total) * 100
    filled = int(bar_length * current // total) if total > 0 else 0
    bar = "=" * filled + "-" * (bar_length - filled)
    print(f"\r[{bar}] {percentage:.1f}% ({current}/{total})", end="", flush=True)
    sys.stdout.flush()


class GitRepository(object):
    """
    git仓库管理
    """

    def __init__(self, local_path, repo_url, branch="master"):
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

        git_local_path = os.path.join(self.local_path, ".git")
        if not is_git_dir(git_local_path):
            print("Cloning repository...")
            try:
                # 先用 --no-checkout 克隆，避免因LFS未初始化而失败
                clone_cmd = [
                    git_exe,
                    "clone",
                    "--depth",
                    "1",
                    "-b",
                    branch,
                    "--no-checkout",
                    repo_url,
                    self.local_path,
                ]
                result = subprocess.run(
                    clone_cmd,
                    shell=False,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                if result.returncode != 0:
                    raise Exception(
                        f"Git clone failed with return code {result.returncode}"
                    )
                print("Repository cloned successfully!")
            except Exception as e:
                print(f"Error cloning repository: {e}")
                raise

            # 拉取 Git LFS 文件
            self._pull_lfs_with_progress()
        else:
            self.repo = Repo(self.local_path)

    def _pull_lfs_with_progress(self):
        """拉取 Git LFS 文件，显示进度"""
        try:
            # 安装 LFS 钩子
            print("\nInitializing Git LFS...")
            result = subprocess.run(
                [git_exe, "-C", self.local_path, "lfs", "install"],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                print(f"Warning: LFS install failed: {result.stderr}")
            else:
                print("Git LFS initialization completed!")

            # 使用 git lfs pull 一步完成：checkout + fetch + checkout
            # 这避免了分开执行时可能的 LFS 初始化问题
            print("\nFetching and checking out repository files with LFS support...")
            print("(This may take a few minutes depending on file size...)", flush=True)
            sys.stdout.flush()

            pull_result = subprocess.run(
                [git_exe, "-C", self.local_path, "lfs", "pull"],
                shell=False,
            )

            if pull_result.returncode != 0:
                print(
                    f"\nWarning: git lfs pull failed with return code {pull_result.returncode}"
                )
                print("Attempting fallback: git checkout...")
                # 备用方案：如果 lfs pull 失败，尝试普通 checkout
                checkout_result = subprocess.run(
                    [git_exe, "-C", self.local_path, "checkout"],
                    shell=False,
                )
                if checkout_result.returncode != 0:
                    print(
                        f"Warning: git checkout also failed with return code {checkout_result.returncode}"
                    )
            else:
                print("\nRepository files and LFS objects pulled successfully!")

        except Exception as e:
            print(f"Error pulling LFS files: {e}")
        else:
            self.repo = Repo(self.local_path)

    def reset_hard(self):
        """
        重置仓库到最新状态
        :return:
        """
        print("Resetting repository to latest state...")
        self.repo.git.reset("--hard", "HEAD~1")

    def commit_push_force(self, message="Update release files"):
        """
        强制提交所有更改
        :return:
        """
        # 判断仓库是否有更改
        if self.repo.is_dirty(untracked_files=True):
            print("Committing and pushing changes...")
            self.repo.git.add(A=True)
            self.repo.git.commit("-m", message)
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
        return [
            item.remote_head
            for item in branches
            if item.remote_head
            not in [
                "HEAD",
            ]
        ]

    def commits(self):
        """
        获取所有提交记录
        :return:
        """
        commit_log = self.repo.git.log(
            '--pretty={"commit":"%h","author":"%an","summary":"%s","date":"%cd"}',
            max_count=50,
            date="format:%Y-%m-%d %H:%M",
        )
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
        self.repo.git.reset("--hard", commit)

    def change_to_tag(self, tag):
        """
        切换tag
        :param tag:
        :return:
        """
        self.repo.git.checkout(tag)


def split_file(input_file, output_dir, chunk_size):
    file_size = os.path.getsize(input_file)
    total_chunks = (file_size + chunk_size - 1) // chunk_size

    print("Splitting file...")
    with open(input_file, "rb") as f:
        index = 0
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            # 按0对齐为了保证合并时也是按顺序进行合并
            output_file = os.path.join(output_dir, f"hutb_{index:05d}.dat")
            show_progress_bar(index + 1, total_chunks)
            with open(output_file, "wb") as out:
                out.write(chunk)
            index += 1
    print()  # 换行


def merge_files(input_dir, output_file):
    files = os.listdir(input_dir)
    files = sorted(
        [f for f in files if f.endswith(".dat")],
        key=lambda x: int(x.split("_")[1].split(".")[0]),
    )

    total_files = len(files)
    print("Merging files...")
    with open(output_file, "wb") as out:
        for idx, file_name in enumerate(files, 1):
            file_path = os.path.join(input_dir, file_name)
            show_progress_bar(idx, total_files)
            with open(file_path, "rb") as f:
                chunk = f.read()
                out.write(chunk)
    print()  # 换行


# 解决 shutil.rmtree 删除 .git 文件夹中只读文件的问题
def remove_readonly(func, path, _):
    """清理只读位，然后重新尝试删除"""
    os.chmod(path, stat.S_IWRITE)
    func(path)


if __name__ == "__main__":


    argparser = argparse.ArgumentParser(description=__doc__)
    argparser.add_argument(
        "-r",
        "--repository",
        metavar="R",
        default="release",
        help="HUTB big file repository (release, dependencies, Content) to download (default: release)",
    )
    argparser.add_argument(
        "-u",
        "--upload",
        metavar="U",
        default="none",
        help="Upload to big file repository (release, dependencies, Content) (default: none means no upload, only download)",
    )
    args = argparser.parse_args()

    start = datetime.datetime.now()

    remote_path = f"https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/{args.repository}.git"

    # 如果指定 -upload 参数（上传发行版），则 -r 参数无效
    # d:\hutb\Build\dependencies\prerequisites\miniconda3\envs\hutb\python.exe download_from_git.py -u release
    if args.upload == "release":
        print("Upload to repository: %s" % args.upload)
        local_path = os.path.join(home_dir, "Build", "UE4Carla")  # , 'hutb'
        rep_path = os.path.join(local_path, "release")
        print("Upload path: ", rep_path)
        # 如果之前存在release目录，则删除
        if os.path.exists(rep_path):
            print("Removing previous release directory: ", rep_path)
            shutil.rmtree(rep_path, onerror=remove_readonly)
        repo = GitRepository(rep_path, remote_path)
        # 寻找 local_path 目录下修改日期最新的 .zip 文件
        latest_file = None
        latest_file_name = None
        latest_time = 0
        for file_name in os.listdir(local_path):
            if file_name.endswith(".zip"):
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
            if file_name.endswith(".dat"):
                file_path = os.path.join(rep_path, file_name)
                print("Removing file: ", file_path)
                os.remove(file_path)
        # 将最新的 .zip 重命名为 hutb.zip，并切分成多个 .dat 小文件，上传到 git 仓库中
        # Build/UE4Carla下没有.zip文件需要判断一下，避免报错
        if latest_file is None:
            print("No zip file found in local path: ", local_path)
            sys.exit(1)
        if os.path.exists(os.path.join(local_path, latest_file)):
            # 如果 hutb.zip 已经存在，则先删除
            if os.path.exists(os.path.join(local_path, "hutb.zip"))  and latest_file != "hutb.zip":
                print("Removing existing hutb.zip file: ", os.path.join(local_path, "hutb.zip"))
                os.remove(os.path.join(local_path, "hutb.zip"))
            # 如果 latest_file 不是 hutb.zip，则重命名为 hutb.zip
            if latest_file != "hutb.zip":
                os.rename(
                    os.path.join(local_path, latest_file),
                    os.path.join(local_path, "hutb.zip"),
                )
        if os.path.exists(os.path.join(rep_path, "*.dat")) is False:
            split_file(
                os.path.join(local_path, "hutb.zip"), rep_path, 256 * 1024 * 1024
            )  # 每个小文件最大为 256MB
        repo.commit_push_force(message=latest_file_name)
        print("Upload finished.")
        sys.exit(0)

    print(f"Repository to download: {args.repository}")
    cur_dir = os.getcwd()
    if args.repository == "release":
        save_dir = "hutb"
    else:
        save_dir = args.repository
    local_path = os.path.join(cur_dir, save_dir)  # , 'hutb'
    print("Download path: ", local_path)

    if os.path.exists(local_path):
        # Remove previous download folder
        shutil.rmtree(local_path, onerror=remove_readonly)
    repo = GitRepository(local_path, remote_path)
    # gitpython 库在新机器下载大文件时会出现问题，改为直接调用 git 命令行工具进行下载
    # repo = GitRepository(local_path, remote_path)
    # 问题：Skipping object checkout, Git LFS is not installed for this repository.
    # 解决：git lfs install
    # git_path = os.path.join(script_dir, 'git', 'bin', 'git.exe')
    # clone_cmd = "%s clone %s  %s && cd %s & %s lfs install  && %s lfs pull && cd .." % (git_path, remote_path, local_path, save_dir, git_path, git_path)
    # print("Cloning repository with command: ", clone_cmd)
    # os.system(clone_cmd)


    # 移除工程中不相关的文件
    if os.path.exists(os.path.join(local_path, ".git")):
        print("Remove .git folder and .gitattributes file...")
        shutil.rmtree(os.path.join(local_path, ".git"), onerror=remove_readonly)
        gitattributes_path = os.path.join(local_path, ".gitattributes")
        if os.path.exists(gitattributes_path):
            os.remove(gitattributes_path)

    # 只有发行版仓库才需要进行合并小文件、解压、删除小文件的操作
    if args.repository == "release":
        # 合并小文件
        print("Merge small files...")
        if os.path.exists(os.path.join(local_path, "hutb.zip")) is False:
            merge_files(local_path, os.path.join(local_path, "hutb.zip"))

        # 先于解压删除后缀名为.dat的小文件，防止整个过程占用过多的磁盘空间
        dat_files = [f for f in os.listdir(local_path) if f.endswith(".dat")]
        if dat_files:
            print("Removing .dat small files...")
            total_dat = len(dat_files)
            for idx, file_name in enumerate(dat_files, 1):
                show_progress_bar(idx, total_dat)
                os.remove(os.path.join(local_path, file_name))
            print()  # 换行

        print("Unzipping hutb.zip...")
        f = zipfile.ZipFile(os.path.join(local_path, "hutb.zip"), "r")  # 压缩文件位置
        file_list = f.namelist()
        total_files = len(file_list)
        for idx, file in enumerate(file_list, 1):
            show_progress_bar(idx, total_files)
            f.extract(file, local_path)  # 解压位置
        f.close()
        print()  # 换行
        # if os.path.exists(os.path.join(local_path, "hutb.zip")):
        #     print("Remove hutb.zip...")
        #     os.remove(os.path.join(local_path, "hutb.zip"))

    # 如果在当前目录存在git文件夹，则删除
    # if os.path.exists(os.path.join(script_dir, "git")):
    #     print("Removing existing git directory: ", os.path.join(script_dir, "git"))
    #     shutil.rmtree(os.path.join(script_dir, "git"), onerror=remove_readonly)
    # 如果在当前目录存在git_min.zip文件，则删除
    # if os.path.exists(os.path.join(script_dir, "git_min.zip")):
    #     print(
    #         "Removing existing git_min.zip file: ",
    #         os.path.join(script_dir, "git_min.zip"),
    #     )
    #     os.remove(os.path.join(script_dir, "git_min.zip"))

    cost_time = datetime.datetime.now() - start
    # 当网络带宽足够大时，下载时间大约4-5分钟左右
    print("Download finished, cost: %s" % (cost_time))
    print("Download path: ", local_path)
    
    print("Launch simulator to click the file: %s" % os.path.join(local_path, 'CarlaUE4.exe'))
    print("Press any key to continue...")
    input()
    # kill_process_on_port(2000)  # 下载完成后自动启动CarlaUE4.exe，方便用户查看下载结果
    # if os.path.exists( os.path.join(local_path, 'CarlaUE4.exe') ):
    #     os.system("start "" %s" % os.path.join(local_path, 'CarlaUE4.exe'))  # 启动CarlaUE4.exe
    # time.sleep(15)  # 延时15秒，方便查看命令行输出
