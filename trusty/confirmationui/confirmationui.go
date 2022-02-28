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
                }
        }
    }
    p := &props{}
    if ctx.Config().VendorConfig("IMXPLUGIN").String("BOARD_SOC_TYPE") == "IMX8MQ" {
         p.Target.Android.Cppflags = append(p.Target.Android.Cppflags, "-DENABLE_SECURE_DISPLAY")
         p.Target.Android.Include_dirs = append(p.Target.Android.Include_dirs, "vendor/nxp-opensource/imx/display/display")
         p.Target.Android.Include_dirs = append(p.Target.Android.Include_dirs, "frameworks/native/include")
         p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "nxp.hardware.display@1.0")
         p.Target.Android.Shared_libs = append(p.Target.Android.Shared_libs, "libfsldisplay")
    }
    ctx.AppendProperties(p)
}
