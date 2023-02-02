using System;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace NFCLoginApp.Pages
{
	public partial class App : Application
	{
		public App()
		{
			InitializeComponent();

			Globals.IsAppRunning = true;
			MainPage = new NavigationPage(new DeviceList());
		}

		protected override void OnStart(){}

		protected override void OnSleep(){}

		protected override void OnResume(){}
	}
}
