using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;

// ANDROID EMULATOR PIN: 1234

namespace NFCLoginApp.Pages
{
	[XamlCompilation(XamlCompilationOptions.Compile)]
	public partial class DeviceList : ContentPage
	{

		public DeviceList()
		{
			InitializeComponent();
			Globals.deviceDB.OnDBError += (Exception ex) => DisplayAlert("Database error", ex.Message, "Ok");

			try
			{
				BindingContext = Globals.deviceDB;
				//string err;
				//DisplayAlert("FP Service", DependencyService.Get<IBioAuth>().CheckFPHW(out err) ? "success" : err, "Ok");
			} catch (Exception e)
			{
				DisplayAlert("ERROR", e.Message, "Ok");
			}
		}

		public void RegisterDeviceBtnClick(object sender, EventArgs e)
		{
			DependencyService.Get<IBioAuth>().RequestFPAuth("Add device", "",
				() => Navigation.PushAsync(new AddDevice(), true),
				() => /*DisplayAlert("Auth fail", "", "Ok")*/ { },
				(int ec, string es) => {
					// cancel events
					if (ec != 10 && ec != 13) {
						DisplayAlert($"Auth error ({ec})", es, "Ok");
					}
				}
			);
		}

		void OnDevTap(object sender, EventArgs e)
		{
			var tgr = (TapGestureRecognizer)((StackLayout)sender).GestureRecognizers[0];
			Device device = (Device)tgr.CommandParameter;
			Navigation.PushAsync(new DeviceDetail(device), true);
		}
	}
}