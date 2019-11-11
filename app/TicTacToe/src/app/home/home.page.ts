import { Component } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';


@Component({
  selector: 'app-home',
  templateUrl: 'home.page.html',
  styleUrls: ['home.page.scss'],
})
export class HomePage {
  inputIP: string = "";
  inputPort: string = "";


  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService)
   {}

  onConfirm(){
    console.log('confirm');
    if (this.inputIP.trim().length <= 0 || this.inputPort.trim().length <= 0) {
      this.alertCtrl.create({
        header: 'Error',
        message: 'Please enter a valid IP and port',
        buttons: [{text: 'Ok', role: 'cancel'}]
      }).then(alertEl => {
        alertEl.present();
      });
      console.log("Invalid port or ip");
      return;
    }

    this.connectionServices.setPort(this.inputPort);
    this.connectionServices.setIP(this.inputIP);

    this.router.navigate(['/setup']);
    console.log('IP:', this.connectionServices.getIP(), 'Port:', this.connectionServices.getPort());
  }

}
