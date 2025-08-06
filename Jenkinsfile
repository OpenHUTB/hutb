pipeline {
    agent none

    stages
    {
        stage('windows')
        {
            // 调用时候需要知道本地节点的名字，否则会报错：‘Jenkins’ doesn’t have label ‘windows’
            // 在 Dashboard -> Manager Jenkins -> Nodes(http://172.21.108.56:8080/manage/computer/) 中进行配置
            agent any // { label "windows" }
            environment
            {
                UE4_ROOT = 'C:\\workspace\\UnrealEngine'
            }
            stages
            {

                stage('windows setup')
                {
                    steps
                    {
                        bat """
                            call setEnv64.bat
                            git update-index --skip-worktree Unreal/CarlaUE4/CarlaUE4.uproject
                        """
                        bat """
                            call setEnv64.bat
                            make setup ARGS="--chrono"
                        """
                    }
                }

                stage('windows build')
                {
                    steps
                    {
                        // bat """
                        //     call setEnv64.bat
                        //     make LibCarla
                        // """
                        bat """
                            call setEnv64.bat
                            make PythonAPI
                        """
                        bat """
                            call setEnv64.bat
                            make CarlaUE4Editor ARGS="--chrono"
                        """
                        bat """
                            call setEnv64.bat
                            make plugins
                        """
                    }
                    // post
                    // {
                    //     always
                    //     {
                    //         archiveArtifacts 'PythonAPI/carla/dist/*.egg'
                    //         archiveArtifacts 'PythonAPI/carla/dist/*.whl'
                    //     }
                    // }
                }

                stage('windows retrieve content')
                {
                    steps
                    {
                        bat """
                            call setEnv64.bat
                            call Update.bat
                        """
                    }
                }

                stage('windows package')
                {
                    steps
                    {
                        bat """
                            call setEnv64.bat
                            make package ARGS="--chrono"
                        """
                        bat """
                            call setEnv64.bat
                            make package ARGS="--packages=AdditionalMaps,Town06_Opt,Town07_Opt,Town11,Town12,Town13,Town15 --target-archive=AdditionalMaps"
                        """
                    }
                    // post {
                    //     always {
                    //         archiveArtifacts 'Build/UE4Carla/*.zip'
                    //     }
                    // }
                }


                stage('windows test')
                {
                    steps
                    {
                        // // 和前面的打包测试重复，仅用于保证生成的文件和测试PythonAPI的一致
                        // bat """
                        //     call setEnv64.bat
                        //     make package ARGS="--chrono"
                        // """
                        bat """
                            call setEnv64.bat
                            make check.PythonAPI
                        """
                        bat """
                            call setEnv64.bat
                            make smoke_tests
                        """
                    }
                }


                // 需要配置亚马逊云的ID和访问密钥
                /*
                stage('windows deploy')
                {
                    // when { anyOf { branch "make_deploy_dev"; buildingTag() } }
                    steps {
                        bat """
                            call setEnv64.bat
                            git checkout .
                            make deploy ARGS="--replace-latest"
                        """
                    }
                }
                */

                // 发布 PythonAPI 到 pypi
                // 需要删除 *.egg 文件
                // 测试：make deploy ARGS="--deploy-to-pypi"

            }

            post
            {
                always
                {
                    deleteDir()
                }
            }

        }
    }
    
}