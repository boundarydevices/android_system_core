package confirmation_ui

import (
        "android/soong/android"
        "android/soong/cc"
)

func init() {
    android.RegisterModuleType("confirmationui_defaults", confirmationui_defaults)
}

func confirmationui_defaults() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, imx_confirmationui_defaults)
    return module
}

func imx_confirmationui_defaults(ctx android.LoadHookContext) {
    type props struct {
        Target struct {
                Android struct {
                        Cppflags []string
                        Include_dirs []string
                        Shared_libs []string
                        Header_libs []string
                        Static_libs []string
                }
        }
    }
    p := &props{}
    var soc_type string = ctx.Config().VendorConfig("IMXPLUGIN").String("BOARD_SOC_TYPE")
    if soc_type == "IMX8MQ" || soc_type == "IMX95"{
         p.Target.Android.Cppflags = append(p.Target.Android.Cppflags, "-DENABLE_SECURE_DISPLAY")
         p.Target.Android.Include_dirs = append(p.Target.Android.Include_dirs, "frameworks/native/include")
         p.Target.Android.Include_dirs = append(p.Target.Android.Include_dirs, "frameworks/native/libs/ui/include")
         p.Target.Android.Header_libs = append(p.Target.Android.Header_libs, "android.hardware.graphics.composer3-command-buffer")
         p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "android.hardware.graphics.composer3-V3-ndk")
         p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "android.hardware.graphics.composer@2.1")
         p.Target.Android.Static_libs = append(p.Target.Android.Static_libs, "libaidlcommonsupport")
    }
    ctx.AppendProperties(p)
}
