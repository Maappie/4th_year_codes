class CreatePresses < ActiveRecord::Migration[8.0]
  def change
    create_table :presses do |t|
      t.string :device_id
      t.datetime :pressed_at

      t.timestamps
    end
  end
end
