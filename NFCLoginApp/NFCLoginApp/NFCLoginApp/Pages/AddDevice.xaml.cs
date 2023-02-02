using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Plugin.NFC;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;

// https://github.com/franckbour/Plugin.NFC

namespace NFCLoginApp.Pages
{
	[XamlCompilation(XamlCompilationOptions.Compile)]
	public partial class AddDevice : ContentPage
	{
		public AddDevice()
		{
			InitializeComponent();
		}

		public void RegisterBtnClick(object sender, EventArgs e)
		{
			string deviceName = etrName.Text?.Trim();
			if (deviceName == null || deviceName.Length == 0)
			{
				DisplayAlert("Error", "Name cannot be empty!", "Ok");
				return;
			}

			INFC nfc = CrossNFC.Current;
			if (!nfc.IsEnabled || !nfc.IsAvailable)
			{
				DisplayAlert("NFC Error", "NFC is required for device registration.", "Ok");
				return;
			}
			Globals.deviceDB.NewDeviceName = deviceName;
			Globals.deviceDB.OnDeviceAdded += OnDeviceAdded;

			lblApproach.IsVisible = true;
		}

		void OnDeviceAdded()
		{
			Globals.deviceDB.OnDeviceAdded -= OnDeviceAdded;
			Navigation.PopAsync();
		}
	}
}