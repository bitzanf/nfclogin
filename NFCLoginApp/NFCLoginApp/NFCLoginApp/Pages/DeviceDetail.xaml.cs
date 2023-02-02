using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace NFCLoginApp.Pages
{
	[XamlCompilation(XamlCompilationOptions.Compile)]
	public partial class DeviceDetail : ContentPage
	{
		public DeviceDetail(Device device)
		{
			InitializeComponent();
			BindingContext = device;
		}

		public async void RemoveDeviceButtonEH(object sender, EventArgs e)
		{
			string choice = await DisplayActionSheet("Really?", "Cancel", "Remove");
			if (choice == "Remove")
			{
				Device device = (Device)BindingContext;
				Globals.deviceDB.RemoveDevice(device.Fingerprint);
				await Navigation.PopAsync();
			}
		}
	}
}