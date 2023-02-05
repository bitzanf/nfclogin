using System;
using System.IO;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace NFCLoginApp.Pages
{
	public partial class App : Application
	{
		public App()
		{
			InitializeComponent();
			//promazani starych dat
			/*foreach (FileInfo file in new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData)).GetFiles())
			{
				file.Delete();
			}*/

			Globals.IsAppRunning = true;
			MainPage = new NavigationPage(new DeviceList());
		}

		protected override void OnStart(){}

		protected override void OnSleep(){}

		protected override void OnResume(){}
	}
}
