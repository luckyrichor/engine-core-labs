# Grant or revoke SeLockMemoryPrivilege ("Lock pages in memory") for one account.
# Windows Home has no secpol.msc, so this calls the LSA API directly.
# Needs an elevated (administrator) PowerShell. The change applies to new logons only:
# sign out and back in (or open a new SSH session) before testing large pages.
#
#   powershell -ExecutionPolicy Bypass -File lock-memory-privilege.ps1 -Action Status
#   powershell -ExecutionPolicy Bypass -File lock-memory-privilege.ps1 -Action Grant
#   powershell -ExecutionPolicy Bypass -File lock-memory-privilege.ps1 -Action Revoke
param(
    [ValidateSet('Status', 'Grant', 'Revoke')][string]$Action = 'Status',
    [string]$Account = ""   # default: the account running this script
)
$ErrorActionPreference = 'Stop'

$src = @"
using System;
using System.Runtime.InteropServices;
public static class LsaRights {
    [StructLayout(LayoutKind.Sequential)]
    struct LSA_UNICODE_STRING { public ushort Length; public ushort MaximumLength; public IntPtr Buffer; }
    [StructLayout(LayoutKind.Sequential)]
    struct LSA_OBJECT_ATTRIBUTES { public int Length; public IntPtr RootDirectory; public IntPtr ObjectName;
        public uint Attributes; public IntPtr SecurityDescriptor; public IntPtr SecurityQualityOfService; }
    [DllImport("advapi32.dll")] static extern uint LsaOpenPolicy(IntPtr system, ref LSA_OBJECT_ATTRIBUTES attrs, uint access, out IntPtr handle);
    [DllImport("advapi32.dll")] static extern uint LsaAddAccountRights(IntPtr h, byte[] sid, LSA_UNICODE_STRING[] rights, uint count);
    [DllImport("advapi32.dll")] static extern uint LsaRemoveAccountRights(IntPtr h, byte[] sid, bool all, LSA_UNICODE_STRING[] rights, uint count);
    [DllImport("advapi32.dll")] static extern uint LsaEnumerateAccountRights(IntPtr h, byte[] sid, out IntPtr rights, out uint count);
    [DllImport("advapi32.dll")] static extern uint LsaFreeMemory(IntPtr p);
    [DllImport("advapi32.dll")] static extern uint LsaClose(IntPtr h);
    [DllImport("advapi32.dll")] static extern int LsaNtStatusToWinError(uint status);

    static IntPtr Open() {
        var attrs = new LSA_OBJECT_ATTRIBUTES();
        IntPtr h;
        uint st = LsaOpenPolicy(IntPtr.Zero, ref attrs, 0x00000800 | 0x00000010 | 0x00000001, out h);
        if (st != 0) throw new System.ComponentModel.Win32Exception(LsaNtStatusToWinError(st));
        return h;
    }
    static LSA_UNICODE_STRING Str(string s) {
        var u = new LSA_UNICODE_STRING();
        u.Buffer = Marshal.StringToHGlobalUni(s);
        u.Length = (ushort)(s.Length * 2);
        u.MaximumLength = (ushort)(s.Length * 2 + 2);
        return u;
    }
    public static void Change(byte[] sid, string right, bool grant) {
        IntPtr h = Open();
        try {
            var r = new[] { Str(right) };
            uint st = grant ? LsaAddAccountRights(h, sid, r, 1) : LsaRemoveAccountRights(h, sid, false, r, 1);
            if (st != 0) throw new System.ComponentModel.Win32Exception(LsaNtStatusToWinError(st));
        } finally { LsaClose(h); }
    }
    public static bool Has(byte[] sid, string right) {
        IntPtr h = Open();
        try {
            IntPtr p; uint n;
            uint st = LsaEnumerateAccountRights(h, sid, out p, out n);
            if (st != 0) return false;  // STATUS_OBJECT_NAME_NOT_FOUND: account has no rights at all
            try {
                int size = Marshal.SizeOf(typeof(LSA_UNICODE_STRING));
                for (int i = 0; i < n; i++) {
                    var u = (LSA_UNICODE_STRING)Marshal.PtrToStructure(new IntPtr(p.ToInt64() + i * size), typeof(LSA_UNICODE_STRING));
                    if (Marshal.PtrToStringUni(u.Buffer, u.Length / 2) == right) return true;
                }
                return false;
            } finally { LsaFreeMemory(p); }
        } finally { LsaClose(h); }
    }
}
"@
Add-Type -TypeDefinition $src

if ($Account) {
    $sidObj = (New-Object System.Security.Principal.NTAccount($Account)).Translate([System.Security.Principal.SecurityIdentifier])
} else {
    $me = [System.Security.Principal.WindowsIdentity]::GetCurrent()
    $sidObj = $me.User
    $Account = $me.Name
}
$sid = New-Object byte[] ($sidObj.BinaryLength)
$sidObj.GetBinaryForm($sid, 0)
$right = 'SeLockMemoryPrivilege'

switch ($Action) {
    'Grant'  { [LsaRights]::Change($sid, $right, $true) }
    'Revoke' { [LsaRights]::Change($sid, $right, $false) }
}
"account=$Account sid=$($sidObj.Value) $right assigned=$([LsaRights]::Has($sid, $right))"
