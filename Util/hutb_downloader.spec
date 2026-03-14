# -*- mode: python ; coding: utf-8 -*-


block_cipher = None


a = Analysis(
    ['download_from_git.py'],
    pathex=[],
    binaries=[],
    datas=[("git\\bin\\git.exe", "git\\bin\\"),("git\\bin\\lfs_demo\\.gitattributes", "git\\bin\\lfs_demo\\"),("git\\bin\\lfs_demo\\bash.exe", "git\\bin\\lfs_demo\\"),("git\\bin\\lfs_demo\\.git\\config", "git\\bin\\lfs_demo\\.git\\"),("git\\bin\\lfs_demo\\.git\\HEAD", "git\\bin\\lfs_demo\\.git\\"),("git\\bin\\lfs_demo\\.git\\index", "git\\bin\\lfs_demo\\.git\\"),("git\\bin\\lfs_demo\\.git\\packed-refs", "git\\bin\\lfs_demo\\.git\\"),("git\\bin\\lfs_demo\\.git\\hooks\\post-checkout", "git\\bin\\lfs_demo\\.git\\hooks\\"),("git\\bin\\lfs_demo\\.git\\hooks\\post-commit", "git\\bin\\lfs_demo\\.git\\hooks\\"),("git\\bin\\lfs_demo\\.git\\hooks\\post-merge", "git\\bin\\lfs_demo\\.git\\hooks\\"),("git\\bin\\lfs_demo\\.git\\hooks\\pre-push", "git\\bin\\lfs_demo\\.git\\hooks\\"),("git\\bin\\lfs_demo\\.git\\lfs\\objects\\5a\\60\\5a60c683f467e9dfae873bd7f85cff21bca921646aac1742a136e5000223ce00", "git\\bin\\lfs_demo\\.git\\lfs\\objects\\5a\\60\\"),("git\\bin\\lfs_demo\\.git\\logs\\HEAD", "git\\bin\\lfs_demo\\.git\\logs\\"),("git\\bin\\lfs_demo\\.git\\logs\\refs\\heads\\master", "git\\bin\\lfs_demo\\.git\\logs\\refs\\heads\\"),("git\\bin\\lfs_demo\\.git\\logs\\refs\\remotes\\origin\\HEAD", "git\\bin\\lfs_demo\\.git\\logs\\refs\\remotes\\origin\\"),("git\\bin\\lfs_demo\\.git\\objects\\16\\6e70eafb3885298a181373642808b21ff61640", "git\\bin\\lfs_demo\\.git\\objects\\16\\"),("git\\bin\\lfs_demo\\.git\\objects\\26\\85993012924212468e9888bc2fa06d4586d0ef", "git\\bin\\lfs_demo\\.git\\objects\\26\\"),("git\\bin\\lfs_demo\\.git\\objects\\2b\\80070a7e34734d7914371b9aa4fa04ecc5b48c", "git\\bin\\lfs_demo\\.git\\objects\\2b\\"),("git\\bin\\lfs_demo\\.git\\objects\\34\\cef43442a53db12542ed077495bb3b80d6be43", "git\\bin\\lfs_demo\\.git\\objects\\34\\"),("git\\bin\\lfs_demo\\.git\\objects\\70\\78a9cee2c28c34f82d77d169411548019dce4a", "git\\bin\\lfs_demo\\.git\\objects\\70\\"),("git\\bin\\lfs_demo\\.git\\objects\\79\\e0adbb94e6ce153d2d7b132cb2e3e34e958394", "git\\bin\\lfs_demo\\.git\\objects\\79\\"),("git\\bin\\lfs_demo\\.git\\objects\\b1\\e238bfaa25936bd4b247f1736b8d4cce79ae86", "git\\bin\\lfs_demo\\.git\\objects\\b1\\"),("git\\bin\\lfs_demo\\.git\\objects\\e1\\fa0cce8fb5770da9e9eec2da558ec12df42f9e", "git\\bin\\lfs_demo\\.git\\objects\\e1\\"),("git\\bin\\lfs_demo\\.git\\objects\\f4\\1cade101e32ae1efdf31be7e9a9129ed5a42ca", "git\\bin\\lfs_demo\\.git\\objects\\f4\\"),("git\\bin\\lfs_demo\\.git\\refs\\heads\\master", "git\\bin\\lfs_demo\\.git\\refs\\heads\\"),("git\\bin\\lfs_demo\\.git\\refs\\remotes\\origin\\HEAD", "git\\bin\\lfs_demo\\.git\\refs\\remotes\\origin\\"),("git\\mingw64\\bin\\git-lfs.exe", "git\\mingw64\\bin\\"),("git\\mingw64\\bin\\git.exe", "git\\mingw64\\bin\\"),("git\\mingw64\\bin\\libiconv-2.dll", "git\\mingw64\\bin\\"),("git\\mingw64\\bin\\libintl-8.dll", "git\\mingw64\\bin\\"),("git\\mingw64\\bin\\libpcre2-8-0.dll", "git\\mingw64\\bin\\"),("git\\mingw64\\bin\\libwinpthread-1.dll", "git\\mingw64\\bin\\"),("git\\mingw64\\bin\\zlib1.dll", "git\\mingw64\\bin\\"),("git\\mingw64\\libexec\\git-core\\git-remote-https.exe", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libbrotlicommon.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libbrotlidec.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libcrypto-1_1-x64.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libcurl-4.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libidn2-0.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libnghttp2-14.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libpsl-5.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libssh2-1.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libssl-1_1-x64.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libunistring-5.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\mingw64\\libexec\\git-core\\libzstd.dll", "git\\mingw64\\libexec\\git-core\\"),("git\\usr\\bin\\msys-2.0.dll", "git\\usr\\bin\\"),("git\\usr\\bin\\sh.exe", "git\\usr\\bin\\"),("git\\usr\\etc\\profile.d\\gawk.csh", "git\\usr\\etc\\profile.d\\"),("git\\usr\\etc\\profile.d\\gawk.sh", "git\\usr\\etc\\profile.d\\"),("git\\usr\\ssl\\cert.pem", "git\\usr\\ssl\\"),("git\\usr\\ssl\\ct_log_list.cnf", "git\\usr\\ssl\\"),("git\\usr\\ssl\\ct_log_list.cnf.dist", "git\\usr\\ssl\\"),("git\\usr\\ssl\\openssl.cnf", "git\\usr\\ssl\\"),("git\\usr\\ssl\\openssl.cnf.dist", "git\\usr\\ssl\\"),("git\\usr\\ssl\\certs\\ca-bundle.crt", "git\\usr\\ssl\\certs\\"),("git\\usr\\ssl\\certs\\ca-bundle.trust.crt", "git\\usr\\ssl\\certs\\"),("git\\usr\\ssl\\misc\\CA.pl", "git\\usr\\ssl\\misc\\"),("git\\usr\\ssl\\misc\\tsget", "git\\usr\\ssl\\misc\\"),("git\\usr\\ssl\\misc\\tsget.pl", "git\\usr\\ssl\\misc\\")],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='hutb_downloader',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['hutb_log.ico'],
)
