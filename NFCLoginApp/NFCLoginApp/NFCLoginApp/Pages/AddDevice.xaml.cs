using System;
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

		~AddDevice()
		{
			Globals.deviceDB.OnDeviceAdded -= OnDeviceAdded;
			Globals.ShouldRegsiter = false;
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
			Globals.ShouldRegsiter = true;

			etrName.IsEnabled = false;
			btnRegister.IsEnabled = false;
			lblApproach.IsVisible = true;
		}

		void OnDeviceAdded()
		{
			Globals.deviceDB.OnDeviceAdded -= OnDeviceAdded;
			Navigation.PopAsync();
		}
	}
}