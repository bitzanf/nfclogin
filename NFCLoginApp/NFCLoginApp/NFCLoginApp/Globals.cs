using System.Diagnostics.SymbolStore;
using Xamarin.Forms;

namespace NFCLoginApp
{
	public class Globals
	{
		public static DeviceDB deviceDB = new DeviceDB();
		public static ICryptoServices cryptoServices = DependencyService.Get<ICryptoServices>();
		public static bool IsAppRunning = false;
		public static bool ShouldRegsiter = false;
	}
}
