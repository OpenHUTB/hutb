ARGS=--all

default: help

# root of the project (makefile directory)
export ROOT_PATH=$(CURDIR)/

# for debug (modify to false when commit)
export IS_DEBUG=false

export git_code=T8w6TYB_r71gGTP3A02B

# dependecy install/build directory (rpclib, gtest, boost)
export INSTALLATION_DIR=$(ROOT_PATH)Build/

export BOOST_VERSION=1.86.0
export API_VERSION=2.10.1
export BOOST_INSTALL_FOLDER=${INSTALLATION_DIR}boost-${BOOST_VERSION}-install/
export BOOST_SOURCE_FOLDER=${INSTALLATION_DIR}boost-${BOOST_VERSION}-source/

# Cache package directory
export CACHE_DIR=C:/jenkins/

help:
	@type "${CARLA_BUILD_TOOLS_FOLDER}\Windows.mk.help"

# use PHONY to force next line as command and avoid conflict with folders of the same name
.PHONY: import
import: server
	@"${CARLA_BUILD_TOOLS_FOLDER}/Import.py" $(ARGS)

CarlaUE4Editor: LibCarla osm2odr
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildCarlaUE4.bat" --build $(ARGS)

launch: CarlaUE4Editor
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildCarlaUE4.bat" --launch $(ARGS)

# 将包含Carla插件的虚幻编辑器打包到 Build/UE4Carla/hutb_editor.zip
# editor: CarlaUE4Editor
# for debug
editor:
	@"${CARLA_BUILD_TOOLS_FOLDER}/PackageEditor.bat" --build $(ARGS)

launch-only:
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildCarlaUE4.bat" --launch $(ARGS)

# 最后打包 car 模式下的包，否则会导致后面的自动测中git rev-parse --short HEAD 找不到对应的目录
package: PythonAPI
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildCarlaUE4.bat" --at-least-write-optionalmodules $(ARGS)
	@"${CARLA_BUILD_TOOLS_FOLDER}/Package.bat" --ue-version 4.26 $(ARGS)

.PHONY: docs
docs:
	@doxygen
	@echo "Documentation index at ./Doxygen/html/index.html"

PythonAPI.docs:
	python PythonAPI/docs/doc_gen.py
	cd PythonAPI/docs && python bp_doc_gen.py

clean:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Package.bat" --clean --ue-version 4.26
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildCarlaUE4.bat" --clean
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildPythonAPI.bat" --clean
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildLibCarla.bat" --clean
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildOSM2ODR.bat" --clean

rebuild: setup
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildCarlaUE4.bat" --rebuild
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildLibCarla.bat" --rebuild
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildOSM2ODR.bat" --rebuild
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildPythonAPI.bat" --rebuild


check:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Check.bat" --all


# 仅用于调试
# check.PythonAPI:
check.PythonAPI:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Check.bat" --python-api

# somke testc
check.smoke:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Check.bat" --smoke $(ARGS)

check.upload:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Check.bat" --upload $(ARGS)

check.air:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Check.bat" --air $(ARGS)

check.vr:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Check.bat" --vr $(ARGS)

run-examples:
	@for D in ${CARLA_EXAMPLES_FOLDER}/*; do [ -d "$${D}" ] && make -C $${D} run.only; done

benchmark: LibCarla
	@echo "Not implemented!"

.PHONY: PythonAPI
PythonAPI: LibCarla osm2odr
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildPythonAPI.bat" --py3 $(ARGS)

server: setup
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildLibCarla.bat" --server --generator "$(GENERATOR)"

client: setup
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildLibCarla.bat" --client --generator "$(GENERATOR)"

.PHONY: LibCarla
LibCarla: setup
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildLibCarla.bat" --server --client --generator "$(GENERATOR)" $(ARGS)

setup: downloadplugin
	@"${CARLA_BUILD_TOOLS_FOLDER}/Setup.bat" --boost-toolset msvc-14.3 --generator "$(GENERATOR)" $(ARGS)


.PHONY: Plugins
plugins:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Plugins.bat" $(ARGS)

deploy:
	@"${CARLA_BUILD_TOOLS_FOLDER}/Deploy.bat" $(ARGS)

osm2odr:
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildOSM2ODR.bat" --generator "$(GENERATOR)" --build $(ARGS)

osmrenderer:
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildOSMRenderer.bat"

downloadplugin:
	@"${CARLA_BUILD_TOOLS_FOLDER}/BuildUE4Plugins.bat" --build $(ARGS)
