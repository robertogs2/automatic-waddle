import { Component, OnInit } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';
import {HttpClient, HttpParams} from '@angular/common/http';
import {Media} from '@ionic-native/media/ngx';


@Component({
  selector: 'app-game',
  templateUrl: './game.page.html',
  styleUrls: ['./game.page.scss'],
})
export class GamePage implements OnInit {
  grid_images:string[] = ["assets/3.png", "assets/3.png", "assets/3.png",
                          "assets/3.png", "assets/3.png", "assets/3.png",
                          "assets/3.png", "assets/3.png", "assets/3.png"];
  grid_busy:Number[] = [3,3,3,
                        3,3,3,
                        3,3,3];
  player_1 = "Player 1: " + this.connectionServices.getUser() + "\t";
  player_2 = "Player 2: PC \t";
  player_1_img = "assets/" + this.connectionServices.getSymbol1().toString() + ".png";
  player_2_img = "assets/" + this.connectionServices.getSymbol2().toString() + ".png";
  turn = "Turn: " + this.connectionServices.getUser();
  turn_username = "";
  game_state = "Play";
  gameover_text = "";
  music_file: any;

  id : any;
  dataObject : any;

  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService,
    public http: HttpClient,
    public media: Media) {
    
    console.log("Creating");  
    this.music_file = this.media.create("/android_asset/public/assets/music.mp3"); 
    console.log("Playing");  
    this.music_file.play();

    this.id = setInterval(() => {
      this.http.get('http://' + this.connectionServices.getIP() + ':' + 
      this.connectionServices.getPort() + '/game').subscribe((data:any) => {
      console.log(data);
      this.dataObject  = data;
      this.grid_busy = this.dataObject['Matrix'];
      this.turn = "Turn: " + this.dataObject['Turn'];
      this.turn_username = this.dataObject['Turn'];
      this.player_1 = "Player 1: " + this.dataObject['Username0'] + "\t";
      this.player_2 = "Player 2: " + this.dataObject['Username1'] + "\t";
      this.player_1_img = "assets/" + this.dataObject['Symbol0'] + "_v2.png";
      this.player_2_img = "assets/" + this.dataObject['Symbol1'] + "_v2.png";
      this.game_state = this.dataObject["State"];

      for(var i = 0; i < 9; i++){
        if(this.grid_busy[i] < 4){
          this.grid_images[i] = "assets/" + this.grid_busy[i] + "_v2.png";
        }
        else{
          //someone won
          this.grid_images[i] = "assets/" + this.grid_busy[i] + "_v2_won.png";
        }
      }
      
      //game over blink
      if(this.game_state == "GameOver"){
        if(this.gameover_text == "¡¡¡¡¡GAME OVER!!!!!")
          this.gameover_text = "";
        else if(this.gameover_text == "")
          this.gameover_text = "¡¡¡¡¡GAME OVER!!!!!";
      }

      });
    }, 100);//every second
  }

  onPushedButton(position){
    if(this.game_state == "Play"){
      console.log("Position pushed: " + position);
      if(this.turn_username == this.connectionServices.getUser() && this.grid_busy[position] == 3){
        console.log("Valid position");
        const params = new HttpParams().set('position', position).
                                        set('username', this.connectionServices.getUser());
        this.http.get('http://' + this.connectionServices.getIP() + ':' + 
        this.connectionServices.getPort() + '/move', {params}).subscribe((data:any) => {
          console.log(data);
          this.dataObject = data;
        });
      }
    }
    else{
      this.alertCtrl.create({
        header: 'Confirmation',
        message: 'Game already finished. Please go back and start a new game.',
        buttons: [{text: 'Go back', handler: () => {
                                                    clearInterval(this.id);
                                                    this.http.get('http://' + this.connectionServices.getIP() + ':' + 
                                                    this.connectionServices.getPort() + '/restart').subscribe((data:any) => {
                                                    console.log(data);
                                                    });
                                                    this.music_file.stop();
                                                    this.router.navigate(['/setup']); 
                                                  }
                  },
                  {text: 'Stay', role: 'cancel'}]
      }).then(alertEl => {
        alertEl.present();
      });
    }
  }

  onBack(){
    this.alertCtrl.create({
      header: 'Confirmation',
      message: 'Are you sure to quit the game?',
      buttons: [{text: 'Yes', handler: () => {
                                              clearInterval(this.id);
                                              this.http.get('http://' + this.connectionServices.getIP() + ':' + 
                                              this.connectionServices.getPort() + '/restart').subscribe((data:any) => {
                                              console.log(data);
                                              });
                                              this.music_file.stop();
                                              this.router.navigate(['/setup']); 
                                            }
                }, 
                {text: 'No', role: 'cancel'}]
    }).then(alertEl => {
      alertEl.present();
    });
  }

  ngOnInit() {
  }

}
