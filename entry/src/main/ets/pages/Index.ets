import { hilog } from '@kit.PerformanceAnalysisKit';
import testNapi from 'libentry.so';
import { common } from '@kit.AbilityKit';
import { taskpool } from '@kit.ArkTS';
import fs from '@ohos.file.fs';

const DOMAIN = 0x0000;

@Entry
@Component
struct Index {
  private context = this.getUIContext().getHostContext() as common.UIAbilityContext;

  aboutToAppear(): void {

    let sandboxPath = this.context.filesDir;
    testNapi.transferSandboxPath(sandboxPath);
    testNapi.sendResourceManagerInstance(this.context.resourceManager)
  }

  build() {
    Row() {
      XComponent({ id: 'xcomponentId', type: XComponentType.SURFACE, libraryname: 'entry' })
        .width("100%")
        .height("100%")
        .onAttach(() => {

        })
        .onAppear(() => {

        })
    }
  }
}
